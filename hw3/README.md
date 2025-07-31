# run competition (all maps, all Algos, 1 GM from /sos )
  ./simulator_315634022 --competition \
  game_maps_folder=../maps \
  game_manager=../GameManager/sos/GameManager_315634022.so \
  algorithms_folder=../Algorithm/sos \
  num_threads=1 \
  --verbose


# run comparative (all GMs, 2 Algos, 1 Map from my folder )
./simulator_315634022 --comparative \
  game_map=../maps/example.txt \
  game_managers_folder=../GameManager/sos/ \
  algorithm1=../Algorithm/sos/Algorithm_315634022.so \
  algorithm2=../Algorithm/sos/Algorithm_322996059_211779582_2.so \
  num_threads=1


  ./simulator_315634022 --comparative \
  game_map=../maps/basic.txt \
  game_managers_folder=../GameManager/sos/ \
  algorithm1=../Algorithm/sos/Algorithm_315634022.so \
  algorithm2=../Algorithm/sos/Algorithm_815634022.so \
  num_threads=1

# run comparative (all GMs, 2 Algos, 1 Map from others )
./simulator_315634022 --comparative \
  game_map=../maps/example.txt \
  game_managers_folder=../GameManager/gameManagers-sos/ \
  algorithm1=../Algorithm/algorithms-sos/Algorithm_322213836_212054837.so \
  algorithm2=../Algorithm/algorithms-sos/Algorithm_322573304_322647603.so \
  num_threads=1