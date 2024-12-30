# Almanah News

## 0.12.4 (2025-01-09)

### Added

- Search (<kbd>Ctrl</kbd>-<kbd>F</kbd>) and Quit (<kbd>Ctrl</kbd>-<kbd>Q</kbd>) keyboard shortcuts (!19)
- Nix-based development environment and CI (!29, !25)

### Fixed

- Various deprecations (#9, !12, !13, !16, !17)
- Localization of aplication name (!24)

### Changed

- Application ID and icon name is now rDNS (`org.gnome.Almanah`) instead of just `almanah` (!28, !32, !31)

### Changed dependencies

- GTKSourceView 3 → GTKSourceView 4 (!15, !23)
- Gcr 3 → Gcr 4 (!14)
- `appstream-util` → `appstreamcli` (!22)

### Translations

#### Added

- Georgian
- Hindi
- Icelandic
- Italian

#### Updated

- Basque
- Catalan
- Chinese (China)
- Friulian
- Galician
- Swedish
- Turkish

## 0.12.3 (2021-03-05)

### Bugs fixed

- #4 Font size is too small in text view
- #7 A lot of `-Wincompatible-pointer-types` compiler warnings

### Translation updates

- Catalan

## 0.12.2 (2020-09-03)

- Fix version number (no other changes)

## 0.12.1 (2020-09-03)

- Bump Meson dependency to 0.51 to simplify build system (thanks to Jan Tojnar)

### Bugs fixed

- #3 typo in src/vfs.c errors

### Translation updates

- Catalan
- English (United Kingdom)
- Malay
- Romanian
- Ukrainian

## 0.12.0 (2019-10-07)

- Move encryption support to SQLiteVFS to improve performance and reduce the
  chance of data loss from use of temporary files
- Various GTK version and API updates
  - Migrated from GtkToolbar to GtkHeaderBar
- Various AppData file updates
- Support undo and redo
- Port to Meson
- Port to libecal-2.0

### Bugs fixed

- #1 Meson build claims implicit function declaration warnings
- !1 build: Port to meson
- !4 docs: Port README to Markdown and update it
- !5 event-factories: Fix an incorrect string comparison

### Translation updates

- Arabic
- Bokmål, Norwegian
- Bosnian
- Catalan
- Chinese (China)
- Czech
- Danish
- Dutch
- English (United Kingdom)
- Esperanto
- Finnish
- French
- Friulian
- German
- Greek, Modern (1453-)
- Hungarian
- Indonesian
- Occitan (post 1500)
- Polish
- Portuguese
- Portuguese (Brazil)
- Russian
- Serbian
- Slovenian
- Spanish
- Swedish
- Thai
- Turkish
- Ukrainian

## 0.11.0

### Major changes

- Improved the tagging accesibility.
- Enhanced the diary security.
- Changed the tagging look (new button with a symbolic link in the toolbar and the tag bar shown integrated in the main toolbar).
- Added a new 256x256 icon, that looks better in GNOME Shell.
- Added an AppData file to show a completed information in Software App.

### Update translations

- Brazilian Portuguese (Enrico Nicoletto, Ramon Brandão, Adorilson Bezerra)
- Czech (Marek Černocký)
- Danish (Joe Hansen)
- French (Alexandre Franke)
- Galician (Fran Diéguez)
- Greek (Dimitris Spingos)
- Hungarian (Balázs Úr)
- Indonesian (Andika Triwidada)
- Latvian (Rūdolfs Mazurs)
- Polish (Piotr Drąg)
- Slovenian (Matej Urbančič)
- Spanish (Daniel Mustieles)
- Serbian (Мирослав Николић)

## 0.10.8

Changes from Almanah 0.10.0.

This is a development release for testing purpose in the road to 0.11, so use carefully.

### Major changes

- Main window redesign, see https://live.gnome.org/Almanah_Diary/Design#New_Design
- Tagging support
- Port to GMenu
- Dropped the libedataserverui dependency and embed the ECellRendererColor and ESourceSelector widgets (Thanks to Matthew Barnes)
- Hide the title bar in maximized windows
- Updated GtkSpell 3 support
- Updated EDS dependency to 3.5.91 (Thanks to Philip Withnall)

### Bugs fixed

- Bug 677209 - The CalendarWindow dropdown doesn't displayed in the correct place
- Bug 680845 - Translatable strings for the time in the events list

### Update translations

- cs (Marek Černocký)
- es (Daniel Mustieles)
- gl (Fran Diéguez)
- hu (Balázs Úr)
- id (Andika Triwidada)
- lv (Rūdolfs Mazurs)
- pl (Piotr Drąg)
- pt_BR (Rafael Ferreira)
- ru (Yuri Myasoedov)
- sl (Andrej Žnidaršič and Matej Urbančič)
- sr (Мирослав Николић)

## 0.10.1

### Bug fixed

- Bug 695117 - Almanah doesn't encrypt the database when the application close (see https://bugzilla.gnome.org/show_bug.cgi?id=695117)

## 0.10.0

### Updated dependencies

### Major changes

- Updated EDS events integration to the new 3.5.91 API
- Second phase of the new UI
- Now is set the default file name when the user doing "Print to File"
- Updated HACKING doc with general principles and a security policy

### Bugs fixed

- Bug 683570 - Fails to build against evolution-data-server 3.5.91
- Bug 680845 - Include the event time as new design suggest
- Bug 676765 - Deactivate the events expander when no events and show an events counter
- Bug 676931 - Fix the calendar button/window style
- Bug 676766 - Show the number of events

### Known bugs

- The CalendarWindow dropdown doesn't displayed in the correct place - see https://bugzilla.gnome.org/show_bug.cgi?id=677209

### Updated translations

- cs (Marek Černocký)
- da (Joe Hansen)
- de (Daniel Winzen)
- en_GB (Chris Leonard)
- es (Daniel Mustieles)
- fr (Bruno Brouard)
- gl (Fran Diéguez)
- id (Andika Triwidada)
- lv (Rūdolfs Mazurs)
- pl (Piotr Drąg)
- sl (Matej Urbančič)
- sr (Мирослав Николић)
- ru (Yuri Myasoedov)

## 0.9.0

### Major changes

- Removed ‘definitions’ in favour of hyperlinks
- Move to a new XML-based entry serialisation format (the change should be transparent to users)
- Use the new GApplication single instance mechanism (bumping our GTK+ and GIO dependencies to 3.0.0 and 2.28.0, respectively)
- Various fixes to the import system
- Search is now asynchronous and case-insensitive (thanks to Álvaro Peña)
- Fixed build with --enable-spell-checking and --disable-encryption
- The first phase of a major re-working of the UI to make it more GNOME-3-ish (thanks to Álvaro Peña) — see https://live.gnome.org/Almanah_Diary/Design
- Re-enable spell checking with GTK+3 (which adds a dependency on GtkSpell v3)
- Automatically save the current entry every 5 minutes (instead of just when closing or changing entries)

### Bugs fixed

- Bug 631835 — Support hyperlinks in diary entries
- Bug 622193 — Use single instance
- Bug 647691 — Make search case-insensitive
- Bug 647690 — Make search asynchronous
- Bug 662016 — Create a toolbar menu item with the fonts style
- Bug 662014 — Remove the right panel
- Bug 667263 — Add support for gtkspell-3.0 in configure
- Bug 666801 — Fix build with encryption support
- Bug 671801 — i18n doesn't work
- Bug 669927 — Save current entry after a timeout

### Updated translations

- cs (Marek Černocký)
- da (Joe Hansen)
- de (Mario Blättermann)
- en_GB (Philip Withnall)
- eo (Kristjan SCHMIDT)
- es (Daniel Mustieles)
- eu (Iñaki Larrañaga Murgoitio)
- fr (Claude Paroz, Pierre Henry)
- gl (Fran Diéguez)
- hu (Balázs Úr)
- pl (Mateusz Kacprzak)
- pt_BR (Djavan Fagundes, Gabriel Speckhahn)
- ru (Yuri Myasoedov)
- sl (Andrej Žnidaršič)
- sr (Мирослав Николић)
- tr (Muhammet Kara)
- uk (Sergiy Gavrylov)

## 0.8.0

### Major changes

- Highlight important entries better in the interface
- Add text and database file export support and make import/export asynchronous
- Add support for storing the edit date of entries
- Make database queries less memory-intensive by using iterators
- Drop the F-Spot event support, as they've dropped their D-Bus plugin
- Build system improvements
- Port to GTK+ 3 (requiring spell checking support to be disabled, as GtkSpell
  doesn't support GTK+ 3 yet)
- Port to GSettings
- Add data format versioning support to the database

### Bugs fixed

- Bug 611889 — A little padding in the textaera would be nice
- Bug 572927 — Important entries
- Bug 622887 — Migrate from dbus-glib to glib's GDBus
- Bug 623231 — Dutch translation
- Bug 641481 — Cannot build with gtk3/evolution 2.91.6

### Updated translations

- ca (Jordi Estrada)
- cs (Marek Černocký)
- da (Joe Hansen)
- de (Mario Blättermann, Christian Kirbach)
- el (Μάριος Ζηντίλης, Kostas Papadimas)
- en_GB (Philip Withnall)
- es (Jorge González)
- fr (Claude Paroz)
- gl (Fran Diéguez, marcoslans)
- hu (Gabor Kelemen, György Balló)
- id (Andika Triwidada)
- nl (Heimen)
- pt_BR (Antonio Fernandes C. Neto, Carlos José Pereira)
- ro (Lucian Adrian Grijincu)
- ru (Diana Kuzachenko, Leonid Kanter)
- sl (Andrej Žnidaršič)
- sv (Daniel Nylander)
- zh_CN (YunQiang Su)

## 0.7.2

### Major changes

- Improve key selection
- Updated build infrastructure

### Updated translations

- cs (Marek Cernocky)
- da (Joe Hansen)
- de (Mario Blättermann)
- en_GB (Philip Withnall)
- sl (Andrej Žnidaršič)
- sv (Daniel Nylander)

## 0.7.1

### Major changes

- Fix non-Evolution build

### Updated translations

- sl (Andrej Žnidaršič)

## 0.7.0

### Major changes

- Add a new "Contact" definition type for Evolution contacts
- Several data loss and crasher fixes
- Add an "Insert Time" function
- Add the ability to mark entries as important
- Add a new "F-Spot Photo" event type, to allow listing of a day's photos
- Allow manual date entry to navigate the diary
- Add import support for text files and other Almanah Diary databases
- Lots of UI cleanup and tweaking
- Add line spacing option when printing
- Change website from http://tecnocode.co.uk/projects/almanah to http://live.gnome.org/Almanah_Diary

### Bugs fixed

- Bug 594871 — Add line spacing options
- Bug 599598 — Segmentation fault in debian unstable
- Bug 585646 — Should not translate %X
- Bug 572032 – Allow manual date entry
- Bug 580052 – Not encrypting keeps encrypted database
- Bug 579242 – No error messages for bad keys when closing
- Bug 578063 — Add F-Spot event type
- Bug 578559 — almanah 0.6.0 does not attempt to open diary db until close
- Bug 572544 — Allow time to be inserted into entries
- Bug 572926 — Automatic database backup

### Updated translations

- da (Joe Hansen)
- de (Mario Blättermann)
- el (Kostas Papadimas, Βασίλης Κοντογιάνης)
- en_GB (Jen Ockwell, Philip Withnall)
- es (Jorge González)
- fr (Claude Paroz)
- gl (Fran Diéguez)
- id (Andika Triwidada)
- sl (Andrej Žnidaršič)
- sv (Daniel Nylander)
- zh_CN (Aron Xu)

## 0.6.0

Note that 0.6 removes the old concept of "links", and all stored links will no longer be accessible. They will,
however, remain in the database, and can be accessed using the following commands while Almanah's running:

```
sqlite3 ~/.local/share/diary.db
.headers ON
SELECT * FROM entry_links;
```

### Major changes

- Improve accessibility support so the UI's navigable in both Accerciser and GOK
- Rename "links" to "definitions", and change them such that one "definition" can be used across multiple
  diary entries to catalogue things of importance which are relevant to many diary entries
- Concurrently, introduce "events", which are displayed automatically with each diary entry and aim to show
  what you were doing on that day; Evolution appointments and tasks are currently the only supported events
- Add an --import-mode, which allows entries to be edited regardless of their status and the current date to
  allow, for example, a one-time import of a previous diary into Almanah
- Improve printing support, adding page settings, print preview and a default page margin of 20px

### Bugs fixed

- Bug 567359 — Allow spell checking to be disabled at runtime
- Bug 564706 — Print margins aren't adjustable
- Bug 561106 — Add command line option to import old entries

### Updated translations

- de (Mario Blättermann)
- fr (Pierre Lemaire, Claude Paroz)
- pt_BR (Taylon Silmer, Vladimir Melo)

## 0.5.0

Note that the database format used in 0.5 is not backwards-compatible
with previous versions, so once an entry has been added in 0.5, the
database will not be usable with previous versions of Almanah.

### Major changes

- Complete the name change to "Almanah Diary"
- Update the architecture, moving to GObject
- Make spell checking optional at compile time
- Add text formatting support
- Improve ability to recover from database corruption
- Allow an encryption key to be chosen
- Make window dimensions persistent
- Allow the spelling language to be set via GConf

### Bugs fixed

- Bug 539792 — Allow encryption key to be set
- Bug 546789 — red dots when switched from another day entry where there were
- Bug 543963 — diary.db file location
- Bug 543739 — Wrong icons are used for desktop and about dialog

### Updated translations

- ar (Djihed Afifi)
- en_GB (Philip Withnall)
- es (Jorge Gonzalez Gonzalez)
- fi (Ilkka Tuohela)
- fr (Robert-André Mauchin)
- nb (Kjartan Maraas)
- pt_BR (Fabrício Godoy, Fábio Nogueira)
- sv (Daniel Nylander)
- th (Anuchit Sakulwilailert)

## 0.4.0

### Major changes

- Fix various translation problems, including untranslatable strings and strings containing markup
- Improve some error messages
- Add icon SVG sources
- Fix licencing problems introduced with the change to GPLv3+
- Rename the application to "Almanah Diary" (the data store remains compatible, however)

### Bugs fixed

- Bug 541709 — Developer Guidelines: Avoid markup wherever possible
- Bug 541716 — misspelled word

### Updated translations

- ca (Gil Forcada Codinachs)
- de (Christoph Wickert)
- en_GB (Philip Withnall)
- fr (Claude Paroz)
- oc (Yannig MARCHEGAY)
- pt_BR (Leonardo Ferreira Fontenelle)
- sv (Daniel Nylander)

## 0.3.1

### Major changes

- Update documentation
- Fix the non-encryption build
- Make the search dialogue non-modal
- Fix the desktop file

## 0.3.0

### Major changes

- Update documentation to point to new SVN repository
- Add search functionality
- Add an icon by Jakub Szypulka and a desktop file
- Improvements to encryption support
- Fix a crasher bug if requesting statistics for a database with no entries

### Updated translations

- en_GB (Philip Withnall)
- fr (Jean-François Martin)

## 0.2.0

### Major changes

- Relicensed from GPLv2 to GPLv3
- Add database encryption support
- Add printing support

## Initial release of Diary 0.1.0

### Major changes

- Project created
- Basic editing support
- Spell checking
- "Note", "URI" and "File" link types
