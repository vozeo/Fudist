import os
import numpy as np
import struct
import pandas as pd
from numba import njit
from sklearn.decomposition import PCA

source = './data/'
datasets = ['gist']
datasets_map = {
    'imagenet': (6, 200),
    # 'msong': (6, 1000),
    # 'word2vec': (6, 1000),
    # 'ukbench': (8, 200),
    # 'deep': (8, 1000),
    # 'gist': (8, 1000),
    # 'glove1.2m': (8, 1000),
    # 'sift': (8, 1000),
    # 'tiny5m': (8, 1000),
    # 'uqv':(8,1000),
    # 'glove-100':(4,1000),
    # 'crawl': (6, 1000),
    # 'enron': (8, 1000),
    # 'mnist': (8, 1000),
    # 'cifar': (8, 200),
    # 'sun':(8, 200),
    # 'notre':(8, 200),
    # 'nuswide':(4, 200),
    # 'trevi': (8, 200)
}


datasets_map = {
    # 'imagenet': (6, 200),
    # 'msong': (6, 1000),
    # 'word2vec': (6, 1000),
    # 'ukbench': (8, 200),
    # 'deep': (8, 1000),
    # # 'gist': (8, 1000),
    # # 'glove1.2m': (8, 1000),
    # 'sift': (8, 1000),
    # 'tiny5m': (8, 1000),
    # # 'uqv':(8,1000),
    # 'glove-100':(4,1000),
    # 'crawl': (6, 1000),
    # # 'enron': (8, 1000)
    # 'mnist': (8, 1000),
    'cifar': (8, 200),
    'sun':(8, 200),
    'notre':(8, 200),
    'nuswide':(4, 200),
    'trevi': (8, 200)
}

def read_fvecs(filename, c_contiguous=True):
    fv = np.fromfile(filename, dtype=np.float32)
    if fv.size == 0:
        return np.zeros((0, 0))
    dim = fv.view(np.int32)[0]
    assert dim > 0
    fv = fv.reshape(-1, 1 + dim)
    if not all(fv.view(np.int32)[:, 0] == dim):
        raise IOError("Non-uniform vector sizes in " + filename)
    fv = fv[:, 1:]
    if c_contiguous:
        fv = fv.copy()
    return fv

def to_fvecs(filename, data):
    print(f"Writing File - {filename}")
    with open(filename, 'wb') as fp:
        for y in data:
            d = struct.pack('I', len(y))
            fp.write(d)
            for x in y:
                a = struct.pack('f', x)
                fp.write(a)

def Orthogonal(D):
    G = np.random.randn(D, D).astype('float32')
    Q, _ = np.linalg.qr(G)
    return Q

def svd(D):
    '''
    get the right singular vectors of a D (m *n), where m is very large while n is small
    '''
    m, n = D.shape
    if m > 1000:
        print('m > 1000, use randomized SVD')
        from sklearn.utils.extmath import randomized_svd
        u, s, vh = randomized_svd(D, n_components=n)
    else:
        print('m < 1000, use numpy SVD')
        u, s, vh = np.linalg.svd(D, full_matrices=False)
    return u, s, vh

def check_orthogonal_matrix(A):
    B = np.dot(A, A.T)
    C = np.eye(A.shape[0])
    return np.allclose(B, C)
    
    
def check_same_matrix(A, B):
    return np.allclose(A, B)

def to_floats(filename, data):
    print(f"Writing File - {filename}")
    with open(filename, 'wb') as fp:
        for y in data:
            a = struct.pack('f', y)
            fp.write(a)

@njit
def calc_approx_dist(X, Q, RealDist):
    # Q: query vector
    # X_code: PQ encoded base vector
    # RealDist: real distance between Q and X
    # initialize a matrix: result, the row number = Q.shape[1], the column number = Q.shape[0] * X.shape[0]
    result = np.zeros((Q.shape[1], Q.shape[0] * X.shape[0]))
        
    lenQ = Q.shape[0]
    lenX = X.shape[0]
        
    for i in range(Q.shape[0]):
        for j in range(X.shape[0]):
            dist = 0
            for k in range(Q.shape[1]):
                dist += (Q[i][k] - X[j][k]) ** 2
                if RealDist[i][j] == 0:
                    if dist == 0:
                        result[k][i * lenX + j] = 1
                else:        
                    result[k][i * lenX + j] = (dist / RealDist[i][j])
    return result



    

if __name__ == "__main__":
    
    for dataset in datasets_map.keys():
        np.random.seed(0)
        
        # path
        path = os.path.join(source, dataset)
        data_path = os.path.join(path, f'{dataset}_base.fvecs')

        # read data vectors
        print(f"Reading {dataset} from {data_path}.")
        X = read_fvecs(data_path)
        D = X.shape[1]

        projection_path = os.path.join(path, 'PCA.fvecs')
        transformed_path = os.path.join(path, f'PCA_{dataset}_base.fvecs')
        

        sampleQuery = datasets_map[dataset][1]
        sampleBase = 10000
        query_path = os.path.join(path, f'{dataset}_query.fvecs')
        dist_path = os.path.join(path, f'Real_Dist_{sampleBase}_{sampleQuery}.fvecs')
        U = read_fvecs(projection_path)
        
        X = read_fvecs(transformed_path)[:sampleBase]
        Q = read_fvecs(query_path)[:sampleQuery]
        Q = np.dot(Q, U)
        RealDist = read_fvecs(dist_path)
        result = calc_approx_dist(X, Q, RealDist)
        # compute mean of each list of result
        result = [np.mean(x) for x in result]
        print(result)
        result_path = os.path.join(path, f'PCA_dist_distrib.floats')
        to_floats(result_path, result)
        
        
