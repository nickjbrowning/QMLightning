'''
Created on 1 Apr 2021

@author: Nicholas J. Browning
@contact: nickjbrowning@gmail.com

@copyright: 

'''

import torch
from qml_lightning.utils.data_format import format_data

    
def get_reductors(X, Z, npcas, elements, reductor_indexes, nbatch, nchoice=512):
    
    reductors = {}
    
    species = torch.from_numpy(elements).float().cuda()
    
    for e in elements:
        
        inputs = []
        
        for i in range(0, len(reductor_indexes), nbatch):
            
            batch_indexes = reductor_indexes[i:i + nbatch] 
            
            coordinates = [X[j] for j in batch_indexes]
            charges = [Z[j] for j in batch_indexes]
            
            v = format_data(coordinates, charges)
            
            coordinates, charges, natom_counts, atomIDs, molIDs = v
            
            gto = get_egto(coordinates, charges, atomIDs, molIDs, natom_counts, species, ngaussians, eta, lmax, rcut, False)
        
            indexes = charges == e
        
            batch_indexes = torch.where(indexes)[0].type(torch.int)
        
            sub = gto[indexes]
        
            if (sub.shape[0] == 0):
                continue
            
            perm = torch.randperm(sub.size(0))
            idx = perm[:nchoice]
    
            choice_input = sub[idx]
            
            inputs.append(choice_input)
        
        if (len(inputs) == 0):
            continue
        
        mat = torch.cat(inputs)

        eigvecs, eigvals, vh = torch.linalg.svd(mat.T, full_matrices=False, compute_uv=True)
    
        cev = 100 - (torch.sum(eigvals) - torch.sum(eigvals[:npcas])) / torch.sum(eigvals) * 100
    
        reductor = eigvecs[:,:npcas]
        size_from = reductor.shape[0]
        size_to = reductor.shape[1]
    
        print (f"{size_from} -> {size_to}  Cumulative Explained Feature Variance = {cev:6.2f} %%")
        
        reductors[e] = reductor
    
    return reductors


def get_reductors(X, charges, npcas, elements):

    reductors = {}
    
    for e in elements:
        
        if (e not in charges):
            continue
        
        indexes = charges == e
        
        sub = X[indexes]
        
        perm = torch.randperm(sub.size(0))
        idx = perm[:512]

        choice_input = sub[idx]

        eigvecs, eigvals, vh = torch.linalg.svd(choice_input.T, full_matrices=False, compute_uv=True)
    
        cev = 100 - (torch.sum(eigvals) - torch.sum(eigvals[:npcas])) / torch.sum(eigvals) * 100
    
        reductor = eigvecs[:,:npcas]
        size_from = reductor.shape[0]
        size_to = reductor.shape[1]
    
        print (f"{size_from} -> {size_to}  Cumulative Explained Feature Variance = {cev:6.2f} %%")
        
        reductors[e] = reductor
    
    return reductors


def project_representation(X, reductor):
    
    '''
    
    projects the representation from shape: 
    nsamples x repsize 
    to 
    nsamples x npcas
    
    '''
    
    return torch.matmul(X, reductor)


def project_derivative(dX, reductor):
    '''
    
    projects the representation derivative from shape:
    
    nsamples x natoms x 3 x repsize 
    to 
    nsamples x natoms x 3 x npcas
    
    '''

    return torch.einsum('jmnk, kl->jmnl', dX, reductor)
