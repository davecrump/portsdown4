#!/usr/bin/env python3

## sudo apt install python3-pil

# eg.
font_name = "dejavu_sans"
font_size = 72
font_file = "DejaVuSans.ttf"

import sys, argparse
from PIL import Image, ImageFont, ImageDraw

parser = argparse.ArgumentParser()
   
parser.add_argument('font_file')
parser.add_argument('font_size', type=int)
parser.add_argument('font_name', help="Must be C variable-name-friendly")

args = parser.parse_args()

try:
    f = open(args.font_file)
    f.close()
except IOError:
    print('Error: Font file is not accessible', file=sys.stderr)
    sys.exit()

font_name = args.font_name
font_size = args.font_size
font_file = args.font_file

font = ImageFont.truetype(font_file, font_size)

print("#include <stdint.h>")
print("#include <stddef.h>")
print("#include \"font.h\"")
font_characters_struct = "static const font_character_t font_%s_%d_characters[] = {\n" % (font_name, font_size);

# Look at ascent/descent
#for ascii_value in range(32, 127):
#    character = chr(ascii_value)
#    character_metrics = font.getoffset(character)
#    print("char %c: ascent: %d, descent: %d" % (character, character_metrics[0], character_metrics[1]))
#abort

number_render_width = 0
# Find numbers render width:
for ascii_value in range(48, 57):
    character = chr(ascii_value)
    character_size = font.getsize(character)
    if(character_size[0] > number_render_width):
        number_render_width = character_size[0]

# Prefill non-printable ascii so we can directly map the array, wastes ~660B of RAM per font file
for ascii_value in range(0, 32):
    font_characters_struct += """
    {
        .height = 0,
        .width = 0,
        .render_width = 0,
        .map = NULL
    },"""

font_height = 0
for ascii_value in range(32, 127):
    character = chr(ascii_value)
    character_size = font.getsize(character)

    if(character_size[1] > font_height):
        font_height = character_size[1]

    image = Image.new('L', character_size, (255))
    draw = ImageDraw.Draw(image)
    draw.text((0, 0), character, font=font)

    print("static const uint8_t font_%s_%d_character_%d[%s] = " % (font_name, font_size, ascii_value, character_size[0] * character_size[1]))
    print("{")
    for y in range(0, character_size[1]):
        print("    ", end='')
        for x in range(0, character_size[0]):
            a = image.getpixel((x, y))
            print("0x%02x" % (255-a), end='')
            if x != (character_size[0]-1):
                print(", ", end='')
            else:
                if y != (character_size[1]-1):
                    print(",")
                else:
                    print("")
    print("};")

    if ascii_value in range(48, 57):
        # Numbers, need constant width
        font_characters_struct += """
    {
        .width = %d,
        .height = %d,
        .render_width = %d,
        .map = font_%s_%d_character_%d
    },""" % ( character_size[0], character_size[1], number_render_width, font_name, font_size, ascii_value)

    else:
        # Not a number
        font_characters_struct += """
    {
        .width = %d,
        .height = %d,
        .render_width = %d,
        .map = font_%s_%d_character_%d
    },""" % ( character_size[0], character_size[1], character_size[0], font_name, font_size, ascii_value)

font_characters_struct = font_characters_struct[:-1] + "\n};"

print(font_characters_struct)

font_ascent = font.getmetrics()[0]

print("""const font_t font_%s_%d = {
    .height = %d,
    .ascent = %d,
    .characters = font_%s_%d_characters
};""" % (font_name, font_size, font_height, font_ascent, font_name, font_size))