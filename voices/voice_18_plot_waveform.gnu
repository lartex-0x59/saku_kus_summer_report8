# voice_18_plot_waveform.gnu
# Usage: gnuplot -persist voice_18_plot_waveform.gnu
set encoding utf8
set datafile separator ','
set title 'Waveform (voice_18_waveform.csv)'
set xlabel 'Time (sec)'
set ylabel 'Amplitude'
set grid
plot 'voice_18_waveform.csv' using 1:2 with lines title 'amplitude'
