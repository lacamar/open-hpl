#!/bin/bash
# fetch-old.sh OUT_DIR: pre-port C++ sources from history (XML layer, config, language, file searcher)
set -e
W=$(git rev-parse --show-toplevel)
OUT=${1:?out dir}
X=aacfd2b^
S=509479b^
mkdir -p $OUT/old/resources $OUT/old/impl
get() { git -C "$W" show "$1:HPL2/core/$2" > "$OUT/old/$(basename "$(dirname "$2")")/$(basename "$2")"; }
for f in include/resources/ConfigFile.h sources/resources/ConfigFile.cpp include/resources/LanguageFile.h sources/resources/LanguageFile.cpp \
         include/resources/XmlDocument.h sources/resources/XmlDocument.cpp include/impl/XmlDocumentTiny.h sources/impl/XmlDocumentTiny.cpp; do
	get $X $f
done
for f in include/resources/FileSearcher.h sources/resources/FileSearcher.cpp; do
	get $S $f
done
