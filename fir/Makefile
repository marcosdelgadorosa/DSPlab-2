all:
	@echo "Targets are:"
	@echo "   fir       Compile fir"
	@echo "   linkports Connect jack client 'simple' to HifiBerry ports"

fir: fir.c 
	gcc fir.c -o fir -ljack

runfir:
	pw-jack ./fir

linkports:
	pw-link simple:output_left alsa_output.platform-soc_sound.stereo-fallback:playback_FL
	pw-link simple:output_right alsa_output.platform-soc_sound.stereo-fallback:playback_FR
	pw-link alsa_input.platform-soc_sound.stereo-fallback:capture_FL simple:input_left
	pw-link alsa_input.platform-soc_sound.stereo-fallback:capture_FR simple:input_right

clean:
	rm -f *~ fir

include ../scripts/pipewire.mk
