Adpla
======

Adpla is a small utility to fade away music, play an advert,
and fade music back in. It does this using JACK for and the
music is passed trough Adpla: it can then operate on the 
music stream as it needs to.

Compiling
---------
Deps JACK and libsndfile:
```
sudo apt-get install libjack-dev libsndfile-dev
```

To compile just `make`.

Usage
-----
Start JACK, then run `adpla` from the command line, start MPlayer (or any other
JACK  capable audio player). Connect like so:
```
MPlayer -> Adpla -> System
```

Done! Adpla will play a file called "advert.wav" every 30 minutes, and fade out
the other music.

```
# change the time to 15 minutes like so
adpla -time 15
```

A demo "advert.wav" is supplied for testing.

Contact
-------
Questions, Queries and Comments:
Harry van Haaren <harryhaaren@gmail.com>
