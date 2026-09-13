# voice_38_plot_spectrum_compare_C2.gnu
# Usage: gnuplot -persist voice_38_plot_spectrum_compare_C2.gnu
# Compare N=4096 and N=16384 spectra, zoomed near the target note(s) (C2).
set encoding utf8
set datafile separator ','
set title 'Zoom near C2 (N=4096 vs N=16384)'
set xlabel 'Frequency (Hz)'
set ylabel 'Magnitude'
set xrange [50.406391:80.406391]
set grid

TARGET_FREQ_0 = 65.406391
set arrow from TARGET_FREQ_0, graph 0 to TARGET_FREQ_0, graph 1 nohead lc rgb 'red' lw 2 dt 2
set label 'C2 (65.41 Hz)' at TARGET_FREQ_0, graph 0.95 tc rgb 'red' offset 1,0

plot 'voice_38_spectrum_N4096.csv' using 1:2 with linespoints pt 7 ps 0.5 title 'N=4096 (df=11.718750Hz)', \
     'voice_38_spectrum_N16384.csv' using 1:2 with linespoints pt 7 ps 0.5 title 'N=16384 (df=2.929688Hz)'
