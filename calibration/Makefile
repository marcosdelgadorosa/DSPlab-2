all:
	@echo "Targets are:"
	@echo "   adc       Compile ADC calibration"
	@echo "   dac       Compile DAC calibration"
	@echo "   runadc    Run ADC calibration jack client"
	@echo "   rundac    Run DAC calibration jack client"
	@echo "   linkports Connect jack client 'simple' to HifiBerry ports"

adc: adc.c
	gcc adc.c -o adc -ljack

dac: dac.c
	gcc dac.c -o dac -ljack

runadc:
	pw-jack ./adc

rundac:
	pw-jack ./dac

linkports:
	pw-link simple:output_left alsa_output.platform-soc_sound.stereo-fallback:playback_FL
	pw-link simple:output_right alsa_output.platform-soc_sound.stereo-fallback:playback_FR
	pw-link alsa_input.platform-soc_sound.stereo-fallback:capture_FL simple:input_left
	pw-link alsa_input.platform-soc_sound.stereo-fallback:capture_FR simple:input_right

clean:
	rm -f *~ dac adc

include ../scripts/pipewire.mk
