# The wifitalkie's case, in build123d.
#   make          exports/*.glb to look at, exports/front.stl and back.stl to print
#   make serve    then open http://localhost:8000/viewer.html

PY ?= /home/farhan/Documents/radiocloud/phone_cad/.venv/bin/python

all:
	$(PY) case.py

serve: all
	python3 -m http.server 8000

clean:
	rm -rf exports

.PHONY: all serve clean
