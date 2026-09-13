# voice_12_plot_spectrum_compare_E4-G4.gnu
# Usage: gnuplot -persist voice_12_plot_spectrum_compare_E4-G4.gnu
# Compare N=4096 and N=16384 spectra, zoomed near the target note(s) (E4+G4).
set encoding utf8
set datafile separator ','
set title 'Zoom near E4+G4 (N=4096 vs N=16384)'
set xlabel 'Frequency (Hz)'
set ylabel 'Magnitude'
set xrange [314.627557:406.995436]
set grid

TARGET_FREQ_0 = 329.627557
set arrow from TARGET_FREQ_0, graph 0 to TARGET_FREQ_0, graph 1 nohead lc rgb 'red' lw 2 dt 2
set label 'E4 (329.63 Hz)' at TARGET_FREQ_0, graph 0.95 tc rgb 'red' offset 1,0
TARGET_FREQ_1 = 392.00
set arrow from TARGET_FREQ_1, graph 0 to TARGET_FREQ_1, graph 1 nohead lc rgb 'red' lw 2 dt 2
set label 'G4 (392.00 Hz)' at TARGET_FREQ_1, graph 0.90 tc rgb 'red' offset 1,0

plot 'voice_12_spectrum_N4096.csv' using 1:2 with linespoints pt 7 ps 0.5 title 'N=4096 (df=11.718750Hz)', \
     'voice_12_spectrum_N16384.csv' using 1:2 with linespoints pt 7 ps 0.5 title 'N=16384 (df=2.929688Hz)'
