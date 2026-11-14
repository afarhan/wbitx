# Plan: contacts, calls by callsign, and text as CW

Agreed with the author, 14 November 2026. Remote operation of one's own
radios, GSM 6.10 and the talkie as a repeater come later.

## The menu

MENU on the home page opens:

    Settings...         WiFi, Link, Sleep move in here, beside the rest
    F1  7074            three frequency slots
    F2  14200
    F3  21090
    VU2XZ     14:32     the callsigns, each with its last PTT or text, UTC
    ...
    Add Callsign...     last; UP on the first item wraps round to it

ENTER on a slot or a callsign opens its menu:

    callsign:   Text, PTT, Read text On/Off, Block On/Off, Delete
    frequency:  Text, PTT, Read text On/Off

- A frequency slot opened takes UP/DOWN as the dial does now,
  accelerating.
- PTT goes back to the home page with that target.
- Delete asks once, as Delete? does now.
- Read text and Block are kept in flash for each target; Read text is On
  by default and Block Off.

## The home page

It shows the target, a callsign or a frequency. PTT talks to it, and
UP/DOWN key Morse to it. ENTER opens the target's menu, which carries
Volume as well.

## The Text page

- It opens scrolling that target's text, UP/DOWN to scroll.
- The PTT switches between scrolling and sending.
- Sending: Morse, each word goes at the word gap (7 units); on the
  keyboard, ENTER held sends the word typed.
- What is sent is shown in the buffer too; a word the server sent to no
  one is marked.

## Incoming text

- Read text On: played as CW in the sidetone at the sender's WPM, held
  while voice is being heard.
- Always added to one buffer in RAM, of about 4 KB, the oldest
  overwritten. A header `VU2XZ>VU2ESE`, or `VU2XZ>7074`, when the sender
  or target changes. Lost at power-off; not kept in flash.
- From a blocked callsign: no voice and no text.

## Calls by callsign

The talkie says its target on the TCP connection, as it says its room.
The server puts two talkies that have each other as target in a room of
their own. Whoever begins to speak takes it, as in any room.

## Time

After WELCOME the server says `TIME <unix seconds>`. The talkie keeps
the difference from `millis()` and adds it back for the time; each
connect sets it again. The time is UTC. Nothing is stamped per word:
each target keeps only the time of its last PTT or text, in or out.

## The wire, on the TCP connection the talkie keeps

    TIME <unix seconds>                        server, after WELCOME
    TARGET <callsign>                          talkie: speak to a callsign
    TARGET <kHz>                               talkie: speak to a room (as now)
    TEXT <target> <wpm> <word>                 talkie, a word sent
    TEXT <from> <target> <wpm> <word>          server, a word for this talkie
    TEXT SENT <N>                              server: it went to N, 0 none

UDP and its packets are unchanged.

## The work, in order

1. Menu rework: slots, callsigns, Add Callsign..., wrap round, Settings
   gaining WiFi, Link and Sleep; the target's menu; the home page with
   a target. Settings gain the three slots and, for each target, Read
   text and Block. (talkie: screen.c, settings.c, contacts.c; tests)
2. Server: TIME, TARGET to a callsign, TEXT and TEXT SENT, Block not
   the server's (the talkie drops). (zbitxre accounts/remote.py and
   voip.py, PROTOCOL.md; test_voip.py) Deployed once its tests pass.
3. Talkie, sending and receiving text: the Text page, the buffer, CW
   played from text (morse.c gives the elements of a word), the time
   from TIME. (talkie: screen.c, service.c, a new text.c; tests)
4. Calls by callsign: TARGET said on connect and on change, the room
   of two on the server.

Each step is built, flashed and tried by the author before the next.
