
cd ..

g++ -o ./src/get_delta0_point ./src/get_delta0_point.cpp -I ./src/ -O3 -mavx2 -fopenmp -lCbcSolver -lCbc -lOsiClp -lOsi -lCoinUtils -lClp

# g++ -pg -o ./src/get_delta0_point ./src/get_delta0_point.cpp -I ./src/ -O3 -mavx2 -fopenmp


echo "Start runnning"
cd src
./get_delta0_point

