# -*- coding: utf-8 -*-
from __future__ import print_function

import io
import json
import logging
import os
import shutil

from os.path import dirname, exists, isdir, join
from subprocess import Popen

from tornado import iostream, gen
from tornado.ioloop import IOLoop

from mod.settings import (
    DEV_HMI, LOG,
    HMI_BAUD_RATE, HMI_SERIAL_PORT, HMI_TIMEOUT,
    LAST_STATE_JSON_FILE,
    PREFERENCES_JSON_FILE,
    SYSTEM_OUTPUT,
    TMP_DIR,
)
from mod.addressings import Addressings
from mod.hmi import HMI, FakeHMI
from mod.host import Host, PEDALBOARD_INSTANCE, PEDALBOARD_INSTANCE_ID
from mod.player import Player
from mod.recorder import Recorder
from mod.screenshot import ScreenshotGenerator
from mod.util import TextFileFlusher, safe_json_load
from mod.bank import list_banks, get_last_bank_and_pedalboard

# ---------------------------------------------------------------------------------------------------------------------

class UserPreferences(object):
    def __init__(self):
        self.prefs = {}
        self.load()

    def get(self, key, default):
        return self.prefs.get(key, default)

    def set(self, key, value):
        self.prefs[key] = value

    def load(self):
        if not exists(PREFERENCES_JSON_FILE):
            self.reset()
            return
        try:
            with open(PREFERENCES_JSON_FILE, "r") as fh:
                self.prefs = json.load(fh)
        except:
            self.reset()

    def reset(self):
        self.prefs = {}
        self.save()

    def save(self):
        with TextFileFlusher(PREFERENCES_JSON_FILE) as fh:
            json.dump(self.prefs, fh, indent=4)


try:
    import rtmidi
except ImportError:
    rtmidi = None

TUNER_MIDI_FILE = "/var/modep/tuner_midi.json"
BANK_NAV_MIDI_FILE = "/var/modep/bank_nav_midi.json"

class TunerMidiListener(object):
    def __init__(self, session):
        self.session = session
        self.learning = False
        self.mapped_channel = -1
        self.mapped_cc = -1
        self.midi_in_instances = []
        self.tuner_open = False
        self.load_mapping()
        self.start_listener()

    def load_mapping(self):
        if os.path.exists(TUNER_MIDI_FILE):
            try:
                with open(TUNER_MIDI_FILE, "r") as f:
                    data = json.load(f)
                    self.mapped_channel = int(data.get("channel", -1))
                    self.mapped_cc = int(data.get("cc", -1))
            except Exception:
                pass

    def save_mapping(self):
        try:
            with open(TUNER_MIDI_FILE, "w") as f:
                json.dump({"channel": self.mapped_channel, "cc": self.mapped_cc}, f)
        except Exception:
            pass

    def start_listener(self):
        if not rtmidi:
            logging.warning("rtmidi not available, TunerMidiListener disabled")
            return
        for inst in self.midi_in_instances:
            try:
                inst.close_port()
            except Exception:
                pass
        self.midi_in_instances = []
        try:
            temp_in = rtmidi.MidiIn()
            ports = temp_in.get_ports()
            del temp_in
            for i, p in enumerate(ports):
                try:
                    m = rtmidi.MidiIn()
                    m.open_port(i)
                    m.set_callback(self.on_midi_message)
                    self.midi_in_instances.append(m)
                    logging.info("TunerMidiListener attached to %s", p)
                except Exception as e:
                    logging.debug("Could not open MIDI port %s: %s", p, e)
        except Exception as e:
            logging.warning("TunerMidiListener init failed: %s", e)

        if not self.midi_in_instances:
            try:
                IOLoop.current().call_later(1.0, self.start_listener)
            except Exception:
                pass

    def on_midi_message(self, event, data=None):
        msg, _ = event
        if len(msg) < 3:
            return
        status = msg[0]
        data1 = msg[1]
        data2 = msg[2]
        cmd = status & 0xf0
        channel = (status & 0x0f) + 1

        if cmd == 0xb0: # Control Change (CC)
            if self.learning:
                self.learning = False
                self.mapped_channel = channel
                self.mapped_cc = data1
                self.save_mapping()
                try:
                    loop = IOLoop.current()
                    loop.add_callback(self.session.msg_callback, "midi_map /pedalboard :tuner %d %d 0.0 1.0" % (channel, data1))
                except Exception:
                    pass
                return

            if self.mapped_cc >= 0 and data1 == self.mapped_cc:
                if self.mapped_channel <= 0 or channel == self.mapped_channel:
                    # Latching footswitch: CC value >= 64 is ON (1.0), < 64 is OFF (0.0)
                    is_on = bool(data2 >= 64)
                    if is_on != self.tuner_open:
                        self.tuner_open = is_on
                        val = 1.0 if self.tuner_open else 0.0
                        try:
                            loop = IOLoop.current()
                            loop.add_callback(self.session.msg_callback, "param_set /pedalboard :tuner %f" % val)
                            if self.tuner_open:
                                loop.add_callback(self.session.host.hmi_tuner_on)
                            else:
                                loop.add_callback(self.session.host.hmi_tuner_off)
                        except Exception:
                            pass

    def start_learning(self):
        self.learning = True

    def set_mapping(self, channel, cc):
        self.learning = False
        self.mapped_channel = int(channel)
        self.mapped_cc = int(cc)
        self.save_mapping()
        try:
            loop = IOLoop.current()
            loop.add_callback(self.session.msg_callback, "midi_map /pedalboard :tuner %d %d 0.0 1.0" % (self.mapped_channel, self.mapped_cc))
        except Exception:
            pass

    def unmap(self):
        self.learning = False
        self.mapped_channel = -1
        self.mapped_cc = -1
        self.save_mapping()


class BankNavMidiListener(object):
    """
    Listens for MIDI CC messages to navigate through pedalboards in the active bank.
    Handles momentary footswitches (rising edge: CC >= 64).
    """
    def __init__(self, session):
        self.session = session
        self.learning_target = None # 'next', 'prev', or None
        self.channel = 0            # 0 = Any channel, 1-16 = specific channel
        self.next_cc = -1
        self.prev_cc = -1
        self.midi_in_instances = []
        self.load_mapping()
        self.start_listener()

    def load_mapping(self):
        if os.path.exists(BANK_NAV_MIDI_FILE):
            try:
                with open(BANK_NAV_MIDI_FILE, "r") as f:
                    data = json.load(f)
                    self.channel = int(data.get("channel", 0))
                    self.next_cc = int(data.get("next_cc", -1))
                    self.prev_cc = int(data.get("prev_cc", -1))
            except Exception:
                pass

    def save_mapping(self):
        try:
            with open(BANK_NAV_MIDI_FILE, "w") as f:
                json.dump({
                    "channel": self.channel,
                    "next_cc": self.next_cc,
                    "prev_cc": self.prev_cc
                }, f, indent=4)
        except Exception:
            pass

    def start_listener(self):
        if not rtmidi:
            logging.warning("rtmidi not available, BankNavMidiListener disabled")
            return
        for inst in self.midi_in_instances:
            try:
                inst.close_port()
            except Exception:
                pass
        self.midi_in_instances = []
        try:
            temp_in = rtmidi.MidiIn()
            ports = temp_in.get_ports()
            del temp_in
            for i, p in enumerate(ports):
                try:
                    m = rtmidi.MidiIn()
                    m.open_port(i)
                    m.set_callback(self.on_midi_message)
                    self.midi_in_instances.append(m)
                    logging.info("BankNavMidiListener attached to %s", p)
                except Exception as e:
                    logging.debug("Could not open MIDI port for BankNav %s: %s", p, e)
        except Exception as e:
            logging.warning("BankNavMidiListener init failed: %s", e)

        if not self.midi_in_instances:
            try:
                IOLoop.current().call_later(2.0, self.start_listener)
            except Exception:
                pass

    def on_midi_message(self, event, data=None):
        msg, _ = event
        if len(msg) < 3:
            return
        status = msg[0]
        data1 = msg[1] # CC number
        data2 = msg[2] # CC value
        cmd = status & 0xf0
        channel = (status & 0x0f) + 1

        if cmd == 0xb0: # Control Change (CC)
            # 1. Learning Mode
            if self.learning_target:
                target = self.learning_target
                self.learning_target = None
                self.channel = channel
                if target == "next":
                    self.next_cc = data1
                elif target == "prev":
                    self.prev_cc = data1
                self.save_mapping()
                try:
                    loop = IOLoop.current()
                    loop.add_callback(
                        self.session.msg_callback,
                        "bank_nav_mapped %s %d %d" % (target, channel, data1)
                    )
                except Exception:
                    pass
                return

            # Channel match check (0 = Omni / any channel)
            if self.channel > 0 and channel != self.channel:
                return

            # Momentary footswitch triggers only on rising edge (press: CC value >= 64)
            if data2 < 64:
                return

            # Check for Next pedalboard trigger
            if self.next_cc >= 0 and data1 == self.next_cc:
                logging.info("BankNav MIDI: NEXT pedalboard triggered by CC %d (val %d)", data1, data2)
                try:
                    loop = IOLoop.current()
                    loop.add_callback(self.session.navigate_bank_pedalboard, 1)
                except Exception as e:
                    logging.error("Failed to trigger navigate_bank_pedalboard(1): %s", e)
                return

            # Check for Previous pedalboard trigger
            if self.prev_cc >= 0 and data1 == self.prev_cc:
                logging.info("BankNav MIDI: PREV pedalboard triggered by CC %d (val %d)", data1, data2)
                try:
                    loop = IOLoop.current()
                    loop.add_callback(self.session.navigate_bank_pedalboard, -1)
                except Exception as e:
                    logging.error("Failed to trigger navigate_bank_pedalboard(-1): %s", e)
                return

    def start_learning(self, target):
        self.learning_target = str(target).lower()

    def set_mapping(self, target, channel, cc):
        self.learning_target = None
        self.channel = int(channel)
        if target == "next":
            self.next_cc = int(cc)
        elif target == "prev":
            self.prev_cc = int(cc)
        self.save_mapping()

    def unmap(self, target):
        self.learning_target = None
        if target == "next":
            self.next_cc = -1
        elif target == "prev":
            self.prev_cc = -1
        self.save_mapping()


class Session(object):
    def __init__(self):
        logging.basicConfig(level=(logging.DEBUG if LOG else logging.WARNING))

        self.prefs = UserPreferences()
        self.player = Player()
        self.recorder = Recorder()
        self.recordhandle = None
        self.external_ui_timer = None

        self.screenshot_generator = ScreenshotGenerator()
        self.websockets = []

        # Used in mod-app to know when the current pedalboard changed
        self.pedalboard_changed_callback = lambda ok,bundlepath,title:None

        # Try to open real HMI
        hmiOpened = False

        if not DEV_HMI:
            self.hmi  = HMI(HMI_SERIAL_PORT, HMI_BAUD_RATE, HMI_TIMEOUT, self.hmi_initialized_cb, self.hmi_reinit_cb)
            hmiOpened = self.hmi.sp is not None

        #print("Using HMI =>", hmiOpened)

        if not hmiOpened:
            self.hmi = FakeHMI(self.hmi_initialized_cb)
            print("Using FakeHMI =>", self.hmi)

        self.host = Host(self.hmi, self.prefs, self.msg_callback)
        self.tuner_midi_listener = TunerMidiListener(self)
        self.bank_nav_midi_listener = BankNavMidiListener(self)

    def signal_save(self):
        # reuse HMI function
        self.host.hmi_save_current_pedalboard(lambda r:None)

    def signal_device_updated(self):
        self.msg_callback("cc-device-updated")

    def signal_disconnect(self):
        sockets = self.websockets
        self.websockets = []
        for ws in sockets:
            ws.close()

    def hmi_initialized_cb(self):
        pass

    def hmi_reinit_cb(self):
        self.host.reinit_hmi()

    # Callback used by host to send messages to websocket
    def msg_callback(self, msg):
        for ws in self.websockets:
            ws.write_message(msg)

    # Same as above, but ignores the originator websocket
    def msg_callback_broadcast(self, msg, curws):
        for ws in self.websockets:
            if ws != curws:
                ws.write_message(msg)

    # -----------------------------------------------------------------------------------------------------------------
    # Bank Navigation Support

    def navigate_bank_pedalboard(self, direction):
        """
        Switches to the next (+1) or previous (-1) pedalboard in the active bank.
        """
        try:
            # Ensure host banks are loaded
            if self.host.userbanks is None:
                self.host.userbanks = list_banks([], True, False)
            if self.host.factorybanks is None:
                self.host.factorybanks = []
            if self.host.alluserpedalboards is None:
                self.host.alluserpedalboards = []
            if self.host.allfactorypedalboards is None:
                self.host.allfactorypedalboards = []

            if not self.host.userbanks:
                logging.warning("navigate_bank_pedalboard: No userbanks found")
                return False

            last_bank, last_pb = get_last_bank_and_pedalboard()
            user_bank_idx = last_bank if (0 <= last_bank < len(self.host.userbanks)) else 0
            bank = self.host.userbanks[user_bank_idx]
            pedalboards = bank.get('pedalboards', [])
            if not pedalboards:
                logging.warning("navigate_bank_pedalboard: Current bank %s is empty", bank.get('title'))
                return False

            # Find index of current loaded pedalboard
            cur_idx = 0
            for idx, pb in enumerate(pedalboards):
                if pb.get('bundle') == last_pb:
                    cur_idx = idx
                    break

            new_idx = (cur_idx + direction) % len(pedalboards)
            target_bank_id = (user_bank_idx + 2) if self.host.supports_factory_banks else (user_bank_idx + 1)

            target_pb = pedalboards[new_idx]
            logging.info("navigate_bank_pedalboard: Bank '%s' switching index %d (%s) -> %d (%s) [target_bank_id=%d]",
                         bank.get('title'), cur_idx, pedalboards[cur_idx].get('title'),
                         new_idx, target_pb.get('title'), target_bank_id)

            def load_cb(ok):
                logging.info("navigate_bank_pedalboard: Load callback result=%r", ok)

            self.host.hmi_load_bank_pedalboard(target_bank_id, new_idx, load_cb, from_hmi=False)
            return True
        except Exception as e:
            logging.error("navigate_bank_pedalboard error: %s", e)
            return False

    def bank_nav_midi_learn(self, target):
        if hasattr(self, "bank_nav_midi_listener"):
            self.bank_nav_midi_listener.start_learning(target)

    def bank_nav_midi_map(self, target, channel, cc):
        if hasattr(self, "bank_nav_midi_listener"):
            self.bank_nav_midi_listener.set_mapping(target, channel, cc)

    def bank_nav_midi_unmap(self, target):
        if hasattr(self, "bank_nav_midi_listener"):
            self.bank_nav_midi_listener.unmap(target)

    def get_bank_nav_status(self):
        last_bank, last_pb = get_last_bank_and_pedalboard()
        user_banks = self.host.userbanks if self.host.userbanks is not None else list_banks([], True, False)
        user_bank_idx = last_bank if (0 <= last_bank < len(user_banks)) else 0
        current_bank = user_banks[user_bank_idx] if user_banks else {"title": "Default", "pedalboards": []}
        
        listener = getattr(self, "bank_nav_midi_listener", None)
        return {
            "channel": listener.channel if listener else 0,
            "next_cc": listener.next_cc if listener else -1,
            "prev_cc": listener.prev_cc if listener else -1,
            "bank_title": current_bank.get("title", ""),
            "current_pb": last_pb or "",
            "pedalboards": current_bank.get("pedalboards", [])
        }

    # -----------------------------------------------------------------------------------------------------------------
    # System Audio & WiFi methods

    def system_reboot(self):
        Popen(["sudo", "reboot"])

    def system_poweroff(self):
        Popen(["sudo", "poweroff"])

    # -----------------------------------------------------------------------------------------------------------------
    # Web methods

    # Add a new plugin, starts enabled (ie, not bypassed)
    def web_add(self, instance, uri, x, y, callback):
        self.host.add_plugin(instance, uri, x, y, callback)

    # Remove a plugin
    def web_remove(self, instance, callback):
        self.host.remove_plugin(instance, callback)

    # Address a plugin parameter
    def web_parameter_address(self, port, actuator_uri, label, minimum, maximum, value,
                              steps, tempo, dividers, page, subpage, coloured, momentary, operational_mode, callback):
        instance, portsymbol = port.rsplit("/",1)
        extras = {
            'tempo': tempo,
            'dividers': dividers,
            'page': page,
            'subpage': subpage,
            'coloured': coloured,
            'momentary': momentary,
            'operational_mode': operational_mode,
        }
        self.host.address(instance, portsymbol, actuator_uri, label, minimum, maximum, value, steps, extras, callback)

    def web_set_sync_mode(self, mode, callback):
        self.host.set_sync_mode(mode, True, False, True, callback)

    # Connect 2 ports
    def web_connect(self, port_from, port_to, callback):
        self.host.connect(port_from, port_to, callback)

    # Disconnect 2 ports
    def web_disconnect(self, port_from, port_to, callback):
        self.host.disconnect(port_from, port_to, callback)

    # Save the current pedalboard
    # returns saved bundle path
    def web_save_pedalboard(self, title, asNew, callback):
        bundlepath, newTitle = self.host.save(title, asNew, callback)
        self.pedalboard_changed_callback(True, bundlepath, title)

        if self.hmi.initialized and self.host.descriptor.get('hmi_set_pb_name', False):
            self.hmi_set_pb_name(newTitle or title)

        self.screenshot_generator.schedule_screenshot(bundlepath)
        return bundlepath, newTitle

    # Get list of Hardware MIDI devices
    # returns (devsInUse, devList, names, midiAggregatedMode)
    def web_get_midi_device_list(self):
        return self.host.get_midi_ports()

    # Set the selected MIDI devices to @a newDevs
    # Will remove or add new JACK ports as needed
    def web_set_midi_devices(self, newDevs, midiAggregatedMode, midiLoopback):
        return self.host.set_midi_devices(newDevs, midiAggregatedMode, midiLoopback)

    # Send a ping to HMI and Websockets
    def web_ping(self, callback):
        if self.hmi.initialized:
            self.hmi.ping(callback)
        else:
            callback(False)
        self.msg_callback("ping")

    # -----------------------------------------------------------------------------------------------------------------
    # Websocket methods

    # A websocket has connected
    def websocket_opened(self, ws, callback):
        self.websockets.append(ws)

        # Notify host if needed
        self.host.open_connection_if_needed(ws)

        # HMI initial setup
        if self.hmi.initialized and not self.host.is_in_empty_state():
            def hmi_callback(ok):
                callback(True)
            self.hmi.ui_con(hmi_callback)
        else:
            callback(True)

    # A websocket has disconnected
    def websocket_closed(self, ws, callback):
        if ws in self.websockets:
            self.websockets.remove(ws)

        # No more websockets open, close session
        if len(self.websockets) == 0:
            if self.external_ui_timer is not None:
                IOLoop.current().remove_timeout(self.external_ui_timer)
                self.external_ui_timer = None

            # Unload any external UI
            if self.host.external_ui_instance is not None:
                self.host.hide_external_ui()

            # Set HMI to initial-state
            if self.hmi.initialized and not self.host.is_in_empty_state():
                def hmi_callback(ok):
                    callback(True)
                self.hmi.ui_dis(hmi_callback)
                return

        callback(True)

    # Called after a plugin parameter changes value
    # Sends the new value to the web and HMI
    def websocket_parameter_change(self, instance, portsymbol, value, actuator_uri):
        if instance.startswith("effect_"):
            instance = instance.replace("effect_","/",1)
        self.msg_callback("param_set %s %s %f" % (instance, portsymbol, value))

        if not self.hmi.initialized:
            return

        instance_id = self.host.mapper.get_id(instance)
        pluginData  = self.host.plugins.get(instance_id, None)

        if pluginData is None:
            return

        if actuator_uri is None:
            port_addressing = pluginData['addressings'].get(portsymbol, None)
            if port_addressing is None:
                return
            actuator_uri = port_addressing['actuator_uri']

        actuator_hw_id = self.host.addressings.get_actuator_hw_id(actuator_uri)
        if actuator_hw_id is not None:
            self.hmi.parameter_set(actuator_hw_id, value, lambda ok: None)

    # Hardware (control-chain) device connected
    def ws_cc_device_connected(self, device_id):
        self.msg_callback("cc_device_connected %s" % device_id)

    # Hardware (control-chain) device disconnected
    def ws_cc_device_disconnected(self, device_id):
        self.msg_callback("cc_device_disconnected %s" % device_id)

    # Client has received our data and is ready for next bundle
    def ws_data_ready(self, counter):
        if self.host.web_data_ready_counter == counter:
            self.host.web_data_ready_ok = True
            self.host.send_output_data_ready(None, None)

    # Set a plugin parameter
    # We use ":bypass" symbol for on/off state
    def ws_parameter_set(self, port, value, ws):
        instance, portsymbol = port.rsplit("/",1)

        if instance == "/pedalboard" and portsymbol == ":tuner":
            is_on = bool(value >= 0.5)
            if hasattr(self, 'tuner_midi_listener') and self.tuner_midi_listener:
                self.tuner_midi_listener.tuner_open = is_on
            if is_on:
                self.host.hmi_tuner_on()
            else:
                self.host.hmi_tuner_off()
        elif portsymbol == ":bypass":
            bvalue = value >= 0.5
            self.host.bypass(instance, bvalue, None)
        else:
            self.host.param_set(port, value, None)

        self.msg_callback_broadcast("param_set %s %s %f" % (instance, portsymbol, value), ws)

    # LV2 patch support
    def ws_patch_get(self, instance, uri, ws):
        self.host.patch_get(instance, uri, ws)

    def ws_patch_set(self, instance, uri, vtype, value, ws):
        self.host.patch_set(instance, uri, vtype, value, ws)

    # Called when moving a plugin icon in canvas
    def ws_plugin_position(self, instance, x, y, ws):
        self.host.set_plugin_position(instance, x, y)
        self.msg_callback_broadcast("plugin_pos %s %i %i" % (instance, x, y), ws)

    # Change canvas/pedalboard size
    def ws_pedalboard_size(self, width, height):
        self.host.set_pedalboard_size(width, height)

    def ws_show_external_ui(self, instance):
        if self.external_ui_timer is not None:
            IOLoop.current().remove_timeout(self.external_ui_timer)
            self.external_ui_timer = None

        if self.host.external_ui_instance is not None:
            self.host.hide_external_ui()

        if instance is not None and len(instance) != 0:
            def show_ui():
                self.host.show_external_ui(instance)
            self.external_ui_timer = IOLoop.current().add_timeout(IOLoop.current().time() + 1.0, show_ui)

    # Special Addressing for Preset/Snapshot and Tuner
    def ws_address_special(self, port, data, callback):
        presets = data.get('presets', {})
        label = data.get('label', '---')
        minimum = float(presets['minimum'])
        maximum = float(presets['maximum'])
        value = float(presets['value'])
        steps = presets['steps']
        actuator_uri = presets['actuator_uri']
        tempo = presets.get('tempo', False)
        dividers = presets.get('dividers', None)
        page = presets.get('page', None)
        subpage = presets.get('subpage', None)
        coloured = presets.get('coloured', None)
        momentary = presets.get('momentary', None)
        operational_mode = presets.get('operationalMode', None)

        self.web_parameter_address(port, actuator_uri, label, minimum, maximum, value, steps, tempo, dividers,
                                   page, subpage, coloured, momentary, operational_mode, callback)

    # -----------------------------------------------------------------------------------------------------------------
    # HMI functions called by host

    def hmi_preset_loaded(self, instance, preset):
        self.msg_callback("preset_loaded %s %s" % (instance, preset))

    def hmi_set_pb_name(self, title):
        if self.hmi.initialized and self.host.descriptor.get('hmi_set_pb_name', False):
            self.hmi.set_pedalboard_name(title, lambda ok: None)

    # -----------------------------------------------------------------------------------------------------------------
    # Reset/Clear current pedalboard, resetting it to default state

    def reset(self, callback):
        def reset_host(ok):
            self.host.reset(None, callback)

        if self.hmi.initialized:
            def set_pb_name(_):
                self.hmi.set_pedalboard_name("", reset_host)
            def clear_ss_name(_):
                self.host.hmi_clear_ss_name(set_pb_name)
            def clear_hmi(_):
                self.hmi.clear(clear_ss_name)
            if self.host.descriptor.get("hmi_bank_navigation", False):
                self.host.setNavigateWithFootswitches(False, clear_hmi)
            else:
                clear_hmi(True)
        else:
            reset_host(True)

        # Update the title in HMI
        self.pedalboard_changed_callback(True, "", "")

    # host commands

    def format_port(self, port):
        if not 'system' in port and not 'effect' in port:
            port = "effect_%s" % port
        return port

    # END host commands

    def tuner_midi_learn(self):
        if hasattr(self, "tuner_midi_listener"):
            self.tuner_midi_listener.start_learning()

    def tuner_midi_map(self, channel, cc):
        if hasattr(self, "tuner_midi_listener"):
            self.tuner_midi_listener.set_mapping(channel, cc)

    def tuner_midi_unmap(self):
        if hasattr(self, "tuner_midi_listener"):
            self.tuner_midi_listener.unmap()

    def tuner_enable(self, mute=False):
        self.host.current_tuner_mute = bool(mute)
        self.host.hmi_tuner_on()

    def tuner_disable(self):
        self.host.hmi_tuner_off()

SESSION = Session()
