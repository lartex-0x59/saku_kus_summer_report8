# voice_4_plot_waveform.gnu
# Usage: gnuplot -persist voice_4_plot_waveform.gnu
set encoding utf8
set datafile separator ','
set title 'Waveform (voice_4_waveform.csv)'
set xlabel 'Time (sec)'
set ylabel 'Amplitude'
set grid
plot 'voice_4_waveform.csv' using 1:2 with lines title 'amplitude'
