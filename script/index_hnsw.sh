
cd ..

efConstruction=500
M=16
data='sift'

# g++ -o ./src/index_hnsw ./src/index_hnsw.cpp -I ./src/ -O3 -mavx2 -fopenmp


echo "Indexing - ${data}"

data_path=./data/${data}
index_path=./data/${data}
postfix="_shuf11"

data_file="${data_path}/${data}_base.fvecs${postfix}"
index_file="${index_path}/${data}_ef${efConstruction}_M${M}.index_plain${postfix}"
./src/index_hnsw -t float -d $data_file -i $index_file -e $efConstruction -m $M

# data_file="${data_path}/O${data}_base.fvecs"
# index_file="${index_path}/O${data}_ef${efConstruction}_M${M}.index"
# ./src/index_hnsw -d $data_file -i $index_file -e $efConstruction -m $M

# data_file="${data_path}/PCA_${data}_base.fvecs"
# index_file="${index_path}/PCA_${data}_ef${efConstruction}_M${M}.index"
# ./src/index_hnsw -d $data_file -i $index_file -e $efConstruction -m $M

# data_file="${data_path}/DWT_${data}_base.fvecs"
# index_file="${index_path}/DWT_${data}_ef${efConstruction}_M${M}.index"
# ./src/index_hnsw -d $data_file -i $index_file -e $efConstruction -m $M
