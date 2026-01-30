#!/bin/bash
ile_procesow_made=`cat Logi/generator | grep "generator tworzy petenta" | wc -l`
ile_procesow_sukces=`cat Logi/petent | grep "otrzymal wiadomosc jestes przetworzony" | wc -l`
ile_procesow_fail=`cat Logi/petent | grep "error" | wc -l`

echo "petentow stworzonych: $ile_procesow_made"
echo "petentow przetworzonych: $ile_procesow_sukces"
echo "petentow porazek: $ile_procesow_fail"

# TODO:
# ile procesów się uruchomiło
# ile struktur V zostało