# voice_14_plot_spectrum_compare_Ds4.gnu
# Usage: gnuplot -persist voice_14_plot_spectrum_compare_Ds4.gnu
# Compare N=4096 and N=16384 spectra, zoomed near the target note(s) (D#4).
set encoding utf8
set datafile separator ','
set title 'Zoom near D#4 (N=4096 vs N=16384)'
set xlabel 'Frequency (Hz)'
set ylabel 'Magnitude'
set xrange [296.126984:326.126984]
set grid

TARGET_FREQ_0 = 311.126984
set arrow from TARGET_FREQ_0, graph 0 to TARGET_FREQ_0, graph 1 nohead lc rgb 'red' lw 2 dt 2
set label 'D#4 (311.13 Hz)' at TARGET_FREQ_0, graph 0.95 tc rgb 'red' offset 1,0

plot 'voice_14_spectrum_N4096.csv' using 1:2 with linespoints pt 7 ps 0.5 title 'N=4096 (df=11.718750Hz)', \
     'voice_14_spectrum_N16384.csv' using 1:2 with linespoints pt 7 ps 0.5 title 'N=16384 (df=2.929688Hz)'
