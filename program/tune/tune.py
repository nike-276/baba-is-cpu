song = """
XXX XXX XXX XXX Eb5 XXX B4X Db5
XXX XXX XXX XXX XXX XXX XXX XXX

Db5 Db5 Eb5 XXX XXX XXX XXX XXX
Ab3 Ab4 XXX Gb4 XXX XXX XXX XXX

XXX XXX XXX XXX Eb5 XXX B4X Db5
E3X XXX XXX Gb4 XXX XXX XXX XXX

Db5 Db5 Eb5 Eb5 E5X E5X Eb5 Eb5
Gb3 XXX XXX Gb4 XXX XXX XXX XXX

Eb5 Eb5 Bb5 XXX Bb5 XXX Bb5 Bb5
G3X XXX XXX G4X XXX XXX XXX XXX

Bb5 Bb5 B5X B5X B5X B5X Eb5 Eb5
Ab3 Ab4 XXX Ab4 XXX XXX XXX XXX
"""

lines = song.strip().split("\n\n")
right = [line.split("\n")[0] for line in lines]
left = [line.split("\n")[1] for line in lines]

right_notes = [note for line in right for note in line.split()]
left_notes = [note for line in left for note in line.split()]

right_key = {note: hex(i+1)[2] for i, note in enumerate(set(right_notes) - set(["XXX"]))} | {"XXX": '0'}
left_key = {note: hex(i+1)[2] for i, note in enumerate(set(left_notes) - set(["XXX"]))} | {"XXX": '0'}

if len(right_key) > 16:
    raise ValueError("Too many unique right hand notes to map to digits")

if len(left_key) > 16:
    raise ValueError("Too many unique left hand notes to map to digits")

notes_schem = f"""version 1
name "schematics/notes.schem"
origin 0 0

"""

accidental = lambda x: 'flat' if x == 'b' else 'sharp' if x == '#' else x
sanitize = lambda x: 'no' if x.lower() == 'x' else x

for note, i in right_key.items():
    i = int(i, 16)
    a = sanitize(note[0].lower())
    b = sanitize(accidental(note[1]))
    c = sanitize(note[2])

    notes_schem += f"text 0 {i} {a}\n"
    notes_schem += f"text 1 {i} {b}\n"
    notes_schem += f"text 2 {i} {c}\n"

for note, i in left_key.items():
    i = int(i, 16)
    a = sanitize(note[0].lower())
    b = sanitize(accidental(note[1]))
    c = sanitize(note[2])

    notes_schem += f"text 0 {i+17} {a}\n"
    notes_schem += f"text 1 {i+17} {b}\n"
    notes_schem += f"text 2 {i+17} {c}\n"

with open("out/notes.schem", "w") as f:
    f.write(notes_schem)

right_encoded = [right_key[note] for note in right_notes]
left_encoded = [left_key[note] for note in left_notes]

mapping = {
    'f': 'fruit',
    'e': 'egg',
    'd': 'drink',
    'c': 'cake',
    'b': 'bottle',
    'a': 'algae',
    '9': 'cat',
    '8': 'cog',
    '7': 'cash',
    '6': 'sax',
    '5': 'hand',
    '4': 'dust',
    '3': 'bubble',
    '2': 'scissors',
    '1': 'stick',
    '0': 'donut',
}

song_schem = f"""version 1
name "schematics/song.schem"
origin 0 0

"""

for i, (right, left) in enumerate(zip(right_encoded, left_encoded)):
    song_schem += f"object {i} 0 {mapping[left]} right\n"
    song_schem += f"object {i} 3 {mapping[right]} right\n"

with open("out/song.schem", "w") as f:
    f.write(song_schem)

asm = ""

for right, left in zip(reversed(right_encoded), reversed(left_encoded)):
    if right == '0' and left == '0':
        asm += "sub r0, r0\n"
    else:
        asm += f"movl r0, 0x{right}\n"
        asm += f"movh r0, 0x{left}\n"

    asm += "print r0\n"

asm += "play\n"

with open("tune.asm", "w") as f:
    f.write(asm)