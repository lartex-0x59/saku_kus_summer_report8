# voice_8_plot_spectrum_N16384.gnu
# Usage: gnuplot -persist voice_8_plot_spectrum_N16384.gnu
set encoding utf8
set datafile separator ','
set title 'Spectrum (voice_8_spectrum_N16384.csv)  N=16384'
set xlabel 'Frequency (Hz)'
set ylabel 'Magnitude'
set xrange [0:1000.00]
set grid

TARGET_FREQ_0 = 493.883301
set arrow from TARGET_FREQ_0, graph 0 to TARGET_FREQ_0, graph 1 nohead lc rgb 'red' lw 2 dt 2
set label 'B4 (493.88 Hz)' at TARGET_FREQ_0, graph 0.95 tc rgb 'red' offset 1,0

plot 'voice_8_spectrum_N16384.csv' using 1:2 with lines title 'magnitude (N=16384)'
