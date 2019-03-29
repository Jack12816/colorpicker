# colorpicker

A small tool for X11 that writes the color value on your screen at the cursor
position to stdout, in RGB.

### Usage

Left click to print the pixel color, any other mouse click to quit the program.

#### One Shot

In order to just select one pixel run colorpicker with the `--one-shot` option.
The program will then quit after the first click.

#### Output Format

By default the program prints out the color in RGA format and in hexadecimal.
Here is an example:
```
R:  44, G: 190, B:  78 | Hex: #2CBE4E
```

With the help of the `--short` option you can force colorpicker to just
print out the hexadecimal value. Then you just get: `#2CBE4E`.

#### Color Preview

colorpicker allows you to show a preview of the color the currently hovered
pixel in a reactangle. This reactangle is located at the bottom left of your
screen.  This comes in handy in combination with the one shot option so you
don't pick blindly a color. Just add the `--preview` option for this feature.

#### Examples

```bash
# Pick a color and put the hexadecimal value in your clipboard
$ colorpicker --short --one-shot | xsel -b

# Pick a color with preview and put the hexadecimal value in your clipboard
$ colorpicker --short --one-shot --preview | xsel -b
```

### Dependencies

* GTK/GDK 2.0
* X11
* Xcomposite
* Xfixes
#### debian/Deepin Linux

```bash
sudo apt install libgtk2.0-dev libcairo2-dev libxext-dev libxfixes-dev libxcomposite-dev libx11-dev -y
```
### License

MIT
