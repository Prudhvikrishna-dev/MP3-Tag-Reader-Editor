# MP3 Tag Reader and Editor

A command-line tool written in C that reads and edits the metadata (tags) stored in MP3 files.

## Features

- View tags from ID3v2.3, ID3v2.4 and ID3v1 (ID3v2 is used first if both exist).
- Edit tags in ID3v2.3 and ID3v2.4: title, artist, album, year, comment and genre.
- Reads text in ISO-8859-1, UTF-8 and UTF-16, and shows it as UTF-8.
- Safe editing: changes are written to a temporary file first, and the original is replaced only if everything succeeds.

## Project Files

| File | Purpose |
|---|---|
| `view.c` | Reads and displays ID3v2 and ID3v1 tags |
| `edit.c` | Edits ID3v2 tags |
| `common.h`, `types.h`, `view.h`, `edit.h` | Shared headers and declarations |
| `main.c` | Program entry point and argument handling |

## Build

```bash
gcc *.c -o mp3tag
```

## Usage

### View tags

```bash
./mp3tag -v song.mp3
```

### Edit a tag

```bash
./mp3tag -e -t "New Title" song.mp3
```

## Edit Options

| Option | Tag | ID3v2 Frame |
|---|---|---|
| `-t` | Title | TIT2 |
| `-a` | Artist | TPE1 |
| `-A` | Album | TALB |
| `-y` | Year | TDRC (TYER in v2.3) |
| `-c` | Comment | COMM |
| `-g` | Genre | TCON |

## How It Works

### Viewing

1. Checks the first 3 bytes for `ID3` (ID3v2) and the last 128 bytes for `TAG` (ID3v1).
2. Reads each frame of the ID3v2 tag and stores the supported ones.
3. Prints the title, artist, album, year, comment and genre.

### Editing

1. Shows the current tags.
2. Copies the file to `<filename>.tmp`, replacing the chosen frame (or adding it if missing).
3. Copies the audio data unchanged.
4. Renames the temporary file over the original.

## Limitations

- Editing works only for ID3v2 tags. ID3v1-only files are not edited.
- New text is written as ISO-8859-1 in v2.3 tags, so non-Latin text (such as Telugu or Hindi) will not display correctly there. In v2.4 tags it is written as UTF-8.
- When editing, any extended header and padding in the tag are removed.
- ID3v2.4 footers are not handled when editing.
- UTF-16 characters above U+FFFF (such as emoji) are not shown when viewing.
- Only the first comment (COMM) frame and the first frame of each other type are used.

## AI Usage

The core logic of this project is my own work. I used an AI assistant, Claude (by Anthropic), only for support, such as:

- Adding and simplifying comments and documentation in `view.c` and `edit.c`.
- Help with ID3v2.3 handling in `edit.c` (text encoding byte and year frame TYER).
- Help with extended header handling in `view.c` and `edit.c`.
- Reviewing the code for limitations and possible improvements.
- Assistance in writing this README.

I am responsible for reviewing and testing the final code.
