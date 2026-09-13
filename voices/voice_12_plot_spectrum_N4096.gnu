# voice_12_plot_spectrum_N4096.gnu
# Usage: gnuplot -persist voice_12_plot_spectrum_N4096.gnu
set encoding utf8
set datafile separator ','
set title 'Spectrum (voice_12_spectrum_N4096.csv)  N=4096'
set xlabel 'Frequency (Hz)'
set ylabel 'Magnitude'
set xrange [0:1000.00]
set grid

TARGET_FREQ_0 = 329.627557
set arrow from TARGET_FREQ_0, graph 0 to TARGET_FREQ_0, graph 1 nohead lc rgb 'red' lw 2 dt 2
set label 'E4 (329.63 Hz)' at TARGET_FREQ_0, graph 0.95 tc rgb 'red' offset 1,0
TARGET_FREQ_1 = 391.995436
set arrow from TARGET_FREQ_1, graph 0 to TARGET_FREQ_1, graph 1 nohead lc rgb 'red' lw 2 dt 2
set label 'G4 (392.00 Hz)' at TARGET_FREQ_1, graph 0.90 tc rgb 'red' offset 1,0

plot 'voice_12_spectrum_N4096.csv' using 1:2 with lines title 'magnitude (N=4096)'
