#!/bin/bash
ile_procesow_made=`cat Logi/generator | grep "generator stworzyl petenta" | wc -l`
ile_procesow_sukces=`cat Logi/petent | grep "otrzymal wiadomosc jestes przetworzony" | wc -l`
ile_procesow_fail=`cat Logi/petent | grep "error" | wc -l`

echo "petentow stworzonych: $ile_procesow_made"
echo "petentow przetworzonych: $ile_procesow_sukces"
echo "petentow porazek: $ile_procesow_fail"

echo "uruchomione procesy(powinien być tylko bash i ps):"
ps
echo "pozostałe struktury V(powinny być puste):"
ipcs

# ile_procesow_u1=`cat Logi/urzednik_0 | grep "urzednik wysyla wiadomosc z powrotem" | wc -l`
# ile_procesow_u2=`cat Logi/urzednik_1 | grep "urzednik wysyla wiadomosc z powrotem" | wc -l`
# ile_procesow_u3=`cat Logi/urzednik_2 | grep "urzednik wysyla wiadomosc z powrotem" | wc -l`
# ile_procesow_u4=`cat Logi/urzednik_3 | grep "urzednik wysyla wiadomosc z powrotem" | wc -l`
# ile_procesow_u5=`cat Logi/urzednik_4 | grep "urzednik wysyla wiadomosc z powrotem" | wc -l`
# ile_procesow_u6=`cat Logi/urzednik_5 | grep "urzednik wysyla wiadomosc z powrotem" | wc -l`

# echo "petentow obsluzonych przez urzednik_0: $ile_procesow_u1"
# echo "petentow obsluzonych przez urzednik_1: $ile_procesow_u2"
# echo "petentow obsluzonych przez urzednik_2: $ile_procesow_u3"
# echo "petentow obsluzonych przez urzednik_3: $ile_procesow_u4"
# echo "petentow obsluzonych przez urzednik_4: $ile_procesow_u5"
# echo "petentow obsluzonych przez urzednik_5: $ile_procesow_u6"