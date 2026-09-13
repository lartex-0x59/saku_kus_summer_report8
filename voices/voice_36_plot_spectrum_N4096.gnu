# voice_36_plot_spectrum_N4096.gnu
# Usage: gnuplot -persist voice_36_plot_spectrum_N4096.gnu
set encoding utf8
set datafile separator ','
set title 'Spectrum (voice_36_spectrum_N4096.csv)  N=4096'
set xlabel 'Frequency (Hz)'
set ylabel 'Magnitude'
set xrange [0:2720.91]
set grid

TARGET_FREQ_0 = 2093.004522
set arrow from TARGET_FREQ_0, graph 0 to TARGET_FREQ_0, graph 1 nohead lc rgb 'red' lw 2 dt 2
set label 'C7 (2093.00 Hz)' at TARGET_FREQ_0, graph 0.95 tc rgb 'red' offset 1,0

plot 'voice_36_spectrum_N4096.csv' using 1:2 with lines title 'magnitude (N=4096)'
