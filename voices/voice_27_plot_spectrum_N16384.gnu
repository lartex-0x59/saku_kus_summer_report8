# voice_27_plot_spectrum_N16384.gnu
# Usage: gnuplot -persist voice_27_plot_spectrum_N16384.gnu
set encoding utf8
set datafile separator ','
set title 'Spectrum (voice_27_spectrum_N16384.csv)  N=16384'
set xlabel 'Frequency (Hz)'
set ylabel 'Magnitude'
set xrange [0:1000.00]
set grid

TARGET_FREQ_0 = 261.625565
set arrow from TARGET_FREQ_0, graph 0 to TARGET_FREQ_0, graph 1 nohead lc rgb 'red' lw 2 dt 2
set label 'C4 (261.63 Hz)' at TARGET_FREQ_0, graph 0.95 tc rgb 'red' offset 1,0
TARGET_FREQ_1 = 466.163762
set arrow from TARGET_FREQ_1, graph 0 to TARGET_FREQ_1, graph 1 nohead lc rgb 'red' lw 2 dt 2
set label 'A#4 (466.16 Hz)' at TARGET_FREQ_1, graph 0.90 tc rgb 'red' offset 1,0

plot 'voice_27_spectrum_N16384.csv' using 1:2 with lines title 'magnitude (N=16384)'
