# The wifitalkie's case, in build123d.
#   make          exports/*.glb to look at, exports/front.stl and back.stl to print
#   make serve    then open http://localhost:8000/viewer.html
#   make printed  printed/*.stl, the 3D-printed case for the production board
#   make wearable wearable/*.stl, the small wearable case (oval speaker, 602030, magnets)
#   make wearable2 wearable2/*.stl, the same, the board sawn and the speaker over it
#   make wbitx2   wbitx2/*.stl, wearable2 made a wBitx: the 50 mm speaker above the board
#   make BOARD=pcb    for the wificomms PCB, not the perf-board prototype

PY ?= /home/farhan/Documents/radiocloud/phone_cad/.venv/bin/python

all:
	$(PY) case.py

mold:
	$(PY) mold/mold.py

printed:
	$(PY) printed/printed.py

wearable:
	$(PY) wearable/wearable.py

wearable2:
	$(PY) wearable2/wearable2.py

wbitx2:
	$(PY) wbitx2/wbitx2.py

serve: all
	python3 -m http.server 8000

clean:
	rm -rf exports

.PHONY: all mold printed wearable wearable2 wbitx2 serve clean
