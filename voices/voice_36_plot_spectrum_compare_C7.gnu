# voice_36_plot_spectrum_compare_C7.gnu
# Usage: gnuplot -persist voice_36_plot_spectrum_compare_C7.gnu
# Compare N=4096 and N=16384 spectra, zoomed near the target note(s) (C7).
set encoding utf8
set datafile separator ','
set title 'Zoom near C7 (N=4096 vs N=16384)'
set xlabel 'Frequency (Hz)'
set ylabel 'Magnitude'
set xrange [2078.004522:2108.004522]
set grid

TARGET_FREQ_0 = 2093.004522
set arrow from TARGET_FREQ_0, graph 0 to TARGET_FREQ_0, graph 1 nohead lc rgb 'red' lw 2 dt 2
set label 'C7 (2093.00 Hz)' at TARGET_FREQ_0, graph 0.95 tc rgb 'red' offset 1,0

plot 'voice_36_spectrum_N4096.csv' using 1:2 with linespoints pt 7 ps 0.5 title 'N=4096 (df=11.718750Hz)', \
     'voice_36_spectrum_N16384.csv' using 1:2 with linespoints pt 7 ps 0.5 title 'N=16384 (df=2.929688Hz)'
