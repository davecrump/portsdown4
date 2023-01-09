The bash scripts in this folder can be used to recompile and install updates to 
some of the Portsdown c programs:

guir.sh       runs the scheduler in debug mode and hence rpidatvtouch.c
ubv.sh        compiles and runs the LimeSDR bandviewer
ubva.sh       compiles and runs Airspy bandviewer
ubvp.sh       compiles and runs Pluto bandviewer
ubvr.sh       compiles and runs rtl-sdr bandviewer
ubvw.sh       compiles and runs the experimental waterfall display
udmm.dh       compiles and runs the DMM display
udvb.sh       compiles libdvbmod, compiles DvbTsToIQ and then copies dvb2iq to the bin folder
udvbt.sh      compiles dvb_t_stack, copies it to the bin folder, but does not run it
ugui.sh       compiles rpidatvtouch.c but does not run it
uguir.sh      compiles and runs rpidatvtouch.c with the debug scheduler 
ulm.sh        conpiles LongMynd, but does not run it
ulst.sh       compiles LimeSDR Toolbox and limesdr_dvb and copies the 5 binaries to the bin folder
unf.sh        compiles the noise figure meter and runs it
upm.sh        compiles the power meter and runs it in power meter mode
urptr.sh      compiles the rptr application, stops it and restarts it
usgr.sh       compiles the SigGen and runs it
usw.sh        compiles the sweeper and runs it
uxy.sh        compiles the power meter and runs it in xy mode

alias gui calls   guir.sh
alias ugui calls  uguir.sh
alias udvbt calls udvbt.sh

