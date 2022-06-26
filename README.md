# 🪟🪟 WKilo 🪟🪟

![](https://github.com/JaDogg/WKilo/blob/main/wkilo.gif)

- 🪟 Windows Port of kilo text editor [tutorial source](https://github.com/snaptoken/kilo-src)
- Original tutorial can be found [here](https://viewsourcecode.org/snaptoken/kilo/index.html)
- Which is based on [antirez’s kilo](https://github.com/antirez/kilo)


## 🤔 What was changed
- As per [this issue](https://github.com/microsoft/terminal/issues/8820) use `ReadConsoleA` for reading characters.
  - Naturally I implemented `write` using `WriteConsoleA` also
- Since we do not have the `getline()` method (because it's only in POSIX) I lazily stole it from [here](https://stackoverflow.com/a/47229318/1355145)
- We can get the console `cols, rows` as explained [here](https://stackoverflow.com/a/12642749/1355145).
- Changed the behaviour of `void editorSave()` to simply write the file using a different implementation of write file.


