
all:
	g++ -g ad_play.cxx dsp_adsr.cxx -ljack -lpthread -lsndfile -o adpla

install:
	cp adpla /usr/local/bin
