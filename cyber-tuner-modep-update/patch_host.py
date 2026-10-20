import os

host_py = r"C:\Program Files\MOD Desktop\mod\host.py"

with open(host_py, "r", encoding="utf-8") as f:
    content = f.read()

# 1. Patch kMidiLearnURI
old_learn = """        # MIDI learn is not an actual addressing
        if actuator_uri == kMidiLearnURI:
            if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":
                self.send_notmodified("midi_learn %d mode %f %f" % (TUNER_INSTANCE_ID, minimum, maximum), callback, datatype='boolean')"""

new_learn = """        # MIDI learn is not an actual addressing
        if actuator_uri == kMidiLearnURI:
            if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":
                def after_load(_):
                    self.send_notmodified("midi_learn %d mode %f %f" % (TUNER_INSTANCE_ID, minimum, maximum), callback, datatype='boolean')
                self.send_notmodified("add %s %d" % (TUNER_URI, TUNER_INSTANCE_ID), after_load)"""

content = content.replace(old_learn, new_learn)

# 2. Patch kMidiUnlearnURI
old_unlearn = """        # So we need special casing for unlearn.
        if actuator_uri == kMidiUnlearnURI:
            if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":
                self.send_modified("midi_unmap %d mode" % TUNER_INSTANCE_ID, callback, datatype='boolean')"""

new_unlearn = """        # So we need special casing for unlearn.
        if actuator_uri == kMidiUnlearnURI:
            if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":
                def after_load(_):
                    self.send_modified("midi_unmap %d mode" % TUNER_INSTANCE_ID, callback, datatype='boolean')
                self.send_notmodified("add %s %d" % (TUNER_URI, TUNER_INSTANCE_ID), after_load)"""

content = content.replace(old_unlearn, new_unlearn)

# 3. Patch midi_map
old_map = """                        if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":
                            self.send_modified("midi_map %d mode %i %i %f %f" % (TUNER_INSTANCE_ID, channel, controller, minimum, maximum), callback, datatype='boolean')"""

new_map = """                        if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":
                            def after_load(_):
                                self.send_modified("midi_map %d mode %i %i %f %f" % (TUNER_INSTANCE_ID, channel, controller, minimum, maximum), callback, datatype='boolean')
                            self.send_notmodified("add %s %d" % (TUNER_URI, TUNER_INSTANCE_ID), after_load)"""

content = content.replace(old_map, new_map)

# 4. Patch hmi_tuner_off
old_off = """    def hmi_tuner_off(self, callback):
        logging.debug("hmi tuner off")

        def tuner_removed(_):
            if self.current_tuner_mute:
                self.unmute()
            callback(True)

        self.send_notmodified("remove %d" % TUNER_INSTANCE_ID, tuner_removed)"""

new_off = """    def hmi_tuner_off(self, callback):
        logging.debug("hmi tuner off")

        try:
            hw_old_port = self.hw_tuner_input_port(self.current_tuner_port)
            from mod.utils import disconnect_jack_ports
            disconnect_jack_ports("system:capture_%s" % hw_old_port, "effect_%d:%s" % (TUNER_INSTANCE_ID, TUNER_INPUT_PORT))
        except:
            pass

        def tuner_disabled(_):
            if self.current_tuner_mute:
                self.unmute()
            callback(True)

        self.send_notmodified("param_set %d mode 0.0" % TUNER_INSTANCE_ID, tuner_disabled)"""

content = content.replace(old_off, new_off)

with open(host_py, "w", encoding="utf-8") as f:
    f.write(content)

print("host.py patched.")
