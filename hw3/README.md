final debug for competitions:
- clear logs.
- runnning 
# my Simulator:
[my gm, wokring algos]
  ./simulator_315634022 --competition \
    game_maps_folder=../maps \
    game_manager=../GameManager/sos/GameManager_315634022.so \
    algorithms_folder=../Algorithm/working \
    num_threads=1 \
    --verbose

[my gm, my algos]
  ./simulator_315634022 --competition \
    game_maps_folder=../maps \
    game_manager=../GameManager/sos/GameManager_315634022.so \
    algorithms_folder=../Algorithm/sos \
    num_threads=1 \
    --verbose

[hagai gm, wokring algos]
  ./simulator_315634022 --competition \
    game_maps_folder=../maps \
    game_manager=../GameManager/hagai-gm/GameManager_322996059_211779582.so \
    algorithms_folder=../Algorithm/working \
    num_threads=1 \
    --verbose

[hagai gm, my algos]
  ./simulator_315634022 --competition \
    game_maps_folder=../maps \
    game_manager=../GameManager/hagai-gm/GameManager_322996059_211779582.so \
    algorithms_folder=../Algorithm/sos \
    num_threads=1 \
    --verbose


[dan gm, working algos]
  ./simulator_315634022 --competition \
    game_maps_folder=../maps \
    game_manager=../GameManager/hagai-gm/GameManager_206038929_314620071.so \
    algorithms_folder=../Algorithm/working \
    num_threads=1 \
    --verbose

[dan gm, my algos]
  ./simulator_315634022 --competition \
    game_maps_folder=../maps \
    game_manager=../GameManager/hagai-gm/GameManager_206038929_314620071.so \
    algorithms_folder=../Algorithm/sos \
    num_threads=1 \
    --verbose

# dan simulator:   |   chmod 777 ./simulator_206038929_314620071
[dan gm, my algos]
  ./simulator_206038929_314620071 -competition \
    game_maps_folder=../maps \
    game_manager=../GameManager/hagai-gm/GameManager_206038929_314620071.so \
    algorithms_folder=../Algorithm/sos \
    num_threads=1 \
    -verbose

[dan gm, working algos]
 ./simulator_206038929_314620071 -competition \
    game_maps_folder=../maps \
    game_manager=../GameManager/hagai-gm/GameManager_206038929_314620071.so \
    algorithms_folder=../Algorithm/working \
    num_threads=1 \
    -verbose

[my gm, working algos]
 ./simulator_206038929_314620071 -competition \
    game_maps_folder=../maps \
    game_manager=../GameManager/sos/GameManager_315634022.so \
    algorithms_folder=../Algorithm/working \
    num_threads=1 \
    -verbose

[my gm, my algos]
 ./simulator_206038929_314620071 -competition \
    game_maps_folder=../maps \
    game_manager=../GameManager/sos/GameManager_315634022.so \
    algorithms_folder=../Algorithm/sos \
    num_threads=1 \
    -verbose


[hagai gm, wokring algos]
   ./simulator_206038929_314620071 -competition \
    game_maps_folder=../maps \
    game_manager=../GameManager/hagai-gm/GameManager_322996059_211779582.so \
    algorithms_folder=../Algorithm/working \
    num_threads=1 \
    -verbose

[hagai gm, my algos]
   ./simulator_206038929_314620071 -competition \
    game_maps_folder=../maps \
    game_manager=../GameManager/hagai-gm/GameManager_322996059_211779582.so \
    algorithms_folder=../Algorithm/sos \
    num_threads=1 \
    -verbose



# Overview & Flows:
/Simulator/main.cpp execute with flags to run in 2 diffrent mode
> in main Parsing Arguments from CLI then Running in comparative or competetive mode:
1. Gather maps paths from ./maps folder
2. loads dynamically the GM and Algorithms from .so folders
    using Registrars to keep track of all sos provided.
3. once all dlopen succeed start generating MapViews : SatelliteView from .txt files
4. using a MapData Object to manage all info on current map.
5. enqueue task GM->run(params...)
6. advance inner game logic of match until resolving (if verbose){log file of actions}
7. write results to competition_results_<TS>.txt / comparative_results<TS>.txt

# How to run:
from root directory:
1. run make clean && make
2. cp ./other/Algorithm_815634022.so ./Algorithm/sos
3. cd Simulator

4. copy one of these execution commands: 
> [!WARNING] the ones using algorithm-sos \ gamemanagers-sos  are not working


  ./simulator_315634022 --competition \
  game_maps_folder=../maps \
  game_manager=../GameManager/hagai-gm/GameManager_322996059_211779582.so \
  algorithms_folder=../Algorithm/working \
  num_threads=1 \
  --verbose



# run competition (all maps, all Algos, 1 GM from /sos )
  ./simulator_315634022 --competition \
  game_maps_folder=../maps \
  game_manager=../GameManager/sos/GameManager_315634022.so \
  algorithms_folder=../Algorithm/sos \
  num_threads=1 \
  --verbose

# run competition (all maps, all Algos, 1 GM from /sos )
  ./simulator_315634022 --competition \
  game_maps_folder=../maps \
  game_manager=../GameManager/sos/GameManager_315634022.so \
  algorithms_folder=../Algorithm/algorithm-sos \
  num_threads=1 \
  --verbose


  ./simulator_315634022 --competition \
  game_maps_folder=../maps \
  game_manager=../GameManager/sos/GameManager_315634022.so \
  algorithms_folder=../Algorithm/hagai-algo\
  num_threads=1 \
  --verbose


# run comparative (all GMs, 2 Algos, 1 Map from my folder )
./simulator_315634022 --comparative \
  game_map=../maps/basic.txt \
  game_managers_folder=../GameManager/sos/ \
  algorithm1=../Algorithm/sos/Algorithm_315634022.so \
  algorithm2=../Algorithm/sos/Algorithm_322996059_211779582_2.so \
  num_threads=1


Algorithm_322996059_211779582.so

  ./simulator_315634022 --comparative \
  game_map=../maps/basic.txt \
  game_managers_folder=../GameManager/sos/ \
  algorithm1=../Algorithm/sos/Algorithm_315634022.so \
  algorithm2=../Algorithm/sos/Algorithm_322996059_211779582.so \
  num_threads=1

# run comparative (all GMs, 2 Algos, 1 Map from others )
./simulator_315634022 --comparative \
  game_map=../maps/basic.txt \
  game_managers_folder=../GameManager/gamemanagers-sos/ \
  algorithm1=../Algorithm/algorithms-sos/Algorithm_322213836_212054837.so \
  algorithm2=../Algorithm/algorithms-sos/Algorithm_322573304_322647603.so \
  num_threads=1

# POTENTIAL BUGS
————————————---------------------
0. CELL CONTENT inside my board 
1. BUGGY .SOs 
2. Memory management (structs & classes passing from main e.g MapView : SatlliteView)
3. Abi mismatch: Unify compiling of common libs without -L 
4. RealMap passed byRef :
    std::vector<std::shared_ptr<SatelliteView>> mapViews;
    for (size_t mi = 0; mi < mapViews.size(); ++mi) {
        auto mapViewPtr = mapViews[mi];
        size_t cols     = mapCols[mi],
               rows     = mapRows[mi],
               mSteps   = mapMaxSteps[mi],
               nShells  = mapNumShells[mi];
        const std::string mapFile = maps[mi];
        SatelliteView& realMap = *mapViewPtr;

    pool.enqueue([=,&realMap,&mtx,&results,&algoReg,&gmEntry]() {GameResult gr = gm->run(
                        cols, rows,
                        realMap,
                        mapFile,
                        mSteps, nShells,
                        *p1, algo1Name,
                        *p2, algo2Name,
                        [&](int pi,int ti){ return A.createTankAlgorithm(pi,ti); },
                        [&](int pi,int ti){ return B.createTankAlgorithm(pi,ti); }
                    );}
    }
✅ Why Your Own .so Files Work
Because you're compiling them alongside or with access to the same interface code. ABI compatibility is accidentally preserved due to co-compilation.
When using external .so files:
You’ve violated the One Definition Rule (ODR) across dynamic linkage — the types are visually identical but semantically incompatible.
