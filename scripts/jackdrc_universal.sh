#!/bin/sh
# Universal USB / ALSA Audio Interface Auto-Detection & Config Loader for Patchbox Prime
[ -f /etc/modep-audio.conf ] && . /etc/modep-audio.conf

SAMPLE_RATE=${SAMPLE_RATE:-48000}
BUFFER_SIZE=${BUFFER_SIZE:-128}
PERIODS=${PERIODS:-3}

# Wait up to 3 seconds for USB audio devices to enumerate on boot
for i in 1 2 3; do
    if grep -qiE "usb-audio|EVO|UMC|Scarlett|pisound" /proc/asound/cards 2>/dev/null; then
        break
    fi
    sleep 1
done

if [ -z "$AUDIO_CARD" ] || [ "$AUDIO_CARD" = "auto" ]; then
    AUDIO_CARD=""
    # 1. Prefer any USB-Audio device (Audient EVO 4/8/16, Behringer UMC, Focusrite, etc.)
    USB_ID=$(awk '/^[ 0-9]+ \[.*]: USB-Audio/ { gsub(/\[|\]/, "", $2); print $2; exit }' /proc/asound/cards 2>/dev/null)
    if [ -n "$USB_ID" ]; then
        AUDIO_CARD="hw:${USB_ID}"
    fi
    # 2. Fallback to Pisound HAT
    if [ -z "$AUDIO_CARD" ] && grep -qi "pisound" /proc/asound/cards 2>/dev/null; then
        AUDIO_CARD="hw:pisound"
    fi
fi

# Verify chosen card exists and launch native 32/24/16-bit ALSA driver without forcing -S (shorts)
if [ -n "$AUDIO_CARD" ]; then
    CARD_NAME="${AUDIO_CARD#hw:}"
    if [ -d "/proc/asound/${CARD_NAME}" ]; then
        echo "JACK starting on Universal ALSA Device: ${AUDIO_CARD} (${SAMPLE_RATE}Hz, ${BUFFER_SIZE}f/${PERIODS}p)"
        exec /usr/bin/jackd -t 2000 -R -P 95 -d alsa -d "${AUDIO_CARD}" -r "${SAMPLE_RATE}" -p "${BUFFER_SIZE}" -n "${PERIODS}" -X seq -s
    fi
fi

# Fallback if no external USB/HAT audio interface is plugged in: run lock-free Dummy engine so mixer & tape deck always work
echo "No USB/Duplex ALSA interface detected; starting JACK in Universal Dummy 18x20 mode"
exec /usr/bin/jackd -t 2000 -R -P 95 -d dummy -r "${SAMPLE_RATE}" -p "${BUFFER_SIZE}" -C 18 -P 20
