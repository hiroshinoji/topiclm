#!/usr/bin/env python

from multiprocessing import Process
from itertools import product
import subprocess
import time, os, sys, json

def get_train_test(corpus):
    if corpus == "brown":
        return "/home/noji/data/brown/brown_new.train", "/home/noji/data/brown/brown_new.test"
    elif corpus == "nips":
        return "/home/noji/data/nipstxt/nips_new.train", "/home/noji/data/nipstxt/nips_new.test"
    else:
        return "/home/noji/data/BNC/processed/bnc_500.train", "/home/noji/data/BNC/processed/bnc_500.test"


'''
@param: model  :: string
        params :: dictionary
e.g. model = {HPYTM, DHPYTM, cHPYTM, rescaling}
     params = {"file": "/home/noji/data/brown/brown_new.train",
               "model": "brown.K=50.O=4",
               "O": 4}
'''
def create_train_command(model, params):
    if model == 'rescaling':
        cmd = ['./build/src/unigram_rescaling_train']
    else:
        cmd = ['./build/src/hpy_lda_train']
        if model == "DHPYTM":
            params['g'] = 0
            params['t'] = 0
        elif model == "HPYTM":
            params['g'] = 1
            params['t'] = 0
        else:
            params['g'] = 0
            params['t'] = 1
        if 'O' in params and params['O'] == -1: # infinity-gram
            params['O'] = 8
            params['S'] = 4
        else:       # n-gram => p(stop at that node) == 0
            params['S'] = 0

    for key in sorted(params.iterkeys()):
        cmd += ['-'+key, str(params[key])]
    
    return cmd

def run_train_test(corpus, model, train_params, exp_name):
    # b, n, i in train_params must be set
    burnin = int(train_params['b'])
    num_samples = int(train_params['n'])
    interval = int(train_params['i'])
    
    train_path, test_path = get_train_test(corpus)
    model_name = '.'.join([corpus, model] + [("%s=%s" % (k,v)) for k,v in train_params.items()])
    train_params['m'] = model_name
    train_params['f'] = train_path
    
    cmd = create_train_command(model, train_params)

    print "start", model_name

    try:
        FNULL = open(os.devnull, 'w')
        subprocess.check_call(cmd, stdout=FNULL, stderr=subprocess.STDOUT)
    except:
        print model_name, "failed"
        f = open(exp_name + '.failed', 'a')
        f.write(model_name+'\n')
        f.close()
        return

    model_samples = burnin
    logs = {"model": model_name, "ppls": [], "times": []}
    for i in range(num_samples):
        model_path = model_name + '/model/' + str(model_samples) + '.out'
        log_fn     = model_name + "/test_ppl." + str(model_samples) + ".out"
        f = open(log_fn, 'w')
        start = time.time()

        if model == 'rescaling':
            subprocess.check_call(['./build/src/unigram_rescaling_predict',
                                   '-f', test_path,
                                   '-p', str(10),
                                   '-s', str(10),
                                   '-m', model_path],
                                  stdout =  f)
        else:
            subprocess.check_call(['./build/src/hpy_lda_predict',
                                   '-f', test_path,
                                   '-p', str(10),
                                   '-s', str(10),
                                   '-m', model_path],
                                  stdout =  f)
        finish = time.time()

        ppl = float(subprocess.check_output(['tail', '-n', '1', log_fn]).split(' ')[1])
        logs["ppls"].append(ppl)
        logs["times"].append(float(finish - start))
        model_samples += interval
        f.close()
        
    logs["ave_ppl"] = sum(logs["ppls"]) / float(len(logs["ppls"]))
    logs["ave_time"] = sum(logs["times"]) / float(len(logs["times"]))
    f = open(exp_name + ".json", 'a')
    f.write(json.dumps(logs)+'\n')
    f.close()
    print "end", model_name

def create_jobs(corpora, models, train_params, exp_name):
    '''
    corpora = ["nips", "bnc"]
    models  = ["HPYTM", "DHPYTM"]
    => create: (nips*HPYTM), (nips*DHPYTM), (bnc*HPYTM), (bnc*DHPYTM)
    NOTE! when using infinity-gram, please set O = -1
    '''
    jobs = []
    for corpus in corpora:
        for model in models:
            keys = sorted(train_params.keys())
            param_combs = [dict(zip(keys, prod)) for prod in product(*(train_params[key] for key in keys))]
            for params in param_combs:
                jobs.append(Process(target=run_train_test,
                                    args=(corpus,model,params,exp_name)))
    return jobs

def table_based_effect():
    jobs = create_jobs(corpora      = ['brown', 'nips', 'bnc'],
                       models       = ['HPYTM'],
                       train_params = {'b':[2000],
                                       'n':[1],
                                       'i':[1],
                                       'e':[-1,0],
                                       'K':[50],
                                       'O':[3,4]},
                       exp_name     = 'emnlp.table_based_effect')
    return jobs

def hpytm_token():
    jobs = create_jobs(corpora      = ['brown', 'nips', 'bnc'],
                       models       = ['HPYTM'],
                       train_params = {'b':[500],
                                       'n':[10],
                                       'i':[10],
                                       'e':[0],
                                       'K':[10,50,100],
                                       'O':[3]},
                       exp_name     = 'emnlp.hpytm_token')
    return jobs
    
def hpytm():
    jobs = create_jobs(corpora      = ['brown', 'nips', 'bnc'],
                       models       = ['HPYTM'],
                       train_params = {'b':[500],
                                       'n':[10],
                                       'i':[10],
                                       'e':[-1],
                                       'K':[10,50,100],
                                       'O':[3]},
                       exp_name     = 'emnlp.hpytm')
    return jobs

def dhpytm():
    jobs = create_jobs(corpora      = ['brown', 'nips', 'bnc'],
                       models       = ['DHPYTM'],
                       train_params = {'b':[500],
                                       'n':[10],
                                       'i':[10],
                                       'e':[-1],
                                       'K':[10,50,100],
                                       'O':[3]},
                       exp_name     = 'emnlp.dhpytm')
    return jobs

def chpytm():
    jobs = create_jobs(corpora      = ['brown', 'nips', 'bnc'],
                       models       = ['cHPYTM'],
                       train_params = {'b':[500],
                                       'n':[10],
                                       'i':[10],
                                       'e':[-1],
                                       'K':[10,50,100],
                                       'O':[3]},
                       exp_name     = 'emnlp.chpytm')
    return jobs

def rescaling():
    jobs = create_jobs(corpora      = ['brown', 'nips', 'bnc'],
                       models       = ['rescaling'],
                       train_params = {'b':[1000],
                                       'n':[10],
                                       'i':[10],
                                       'K':[10,50,100],
                                       'O':[3]},
                       exp_name     = 'emnlp.rescaling')
    return jobs

# these 3 experiments are for the table of final result
def bnc_final():
    jobs = create_jobs(corpora      = ['bnc'],
                       models       = ['DHPYTM','cHPYTM'],
                       train_params = {'b':[400],
                                       'n':[10],
                                       'i':[10],
                                       'K':[100],
                                       'O':[4,-1]},
                       exp_name     = 'emnlp.bnc_final')
    return jobs

def rescaling_final():
    jobs = create_jobs(corpora      = ['nips', 'bnc'],
                       models       = ['rescaling'],
                       train_params = {'b':[1000],
                                       'n':[10],
                                       'i':[10],
                                       'K':[100],
                                       'O':[4]},
                       exp_name     = 'emnlp.rescaling_final')
    return jobs

def nips_final():
    jobs = create_jobs(corpora      = ['nips'],
                       models       = ['DHPYTM','cHPYTM'],
                       train_params = {'b':[400],
                                       'n':[10],
                                       'i':[10],
                                       'K':[100],
                                       'O':[4,-1]},
                       exp_name     = 'emnlp.nips_final')
    return jobs

def bnc_light_O3():
    jobs = create_jobs(corpora      = ['bnc'],
                       models       = ['HPYTM','DHPYTM','cHPYTM'],
                       train_params = {'b':[400],
                                       'n':[20],
                                       'i':[10],
                                       'K':[50,100],
                                       'O':[3],
                                       'V':[2]},
                       exp_name     = 'emnlp.bnc_light_O3')
    return jobs

def hpytm_final():
    jobs = create_jobs(corpora      = ['nips','bnc'],
                       models       = ['HPYTM'],
                       train_params = {'b':[400],
                                       'n':[10],
                                       'i':[10],
                                       'K':[100],
                                       'O':[4,-1],
                                       'V':[2]},
                       exp_name     = 'emnlp.hpytm_final')
    return jobs

def hpytm_token():
    jobs = create_jobs(corpora      = ['brown','nips','bnc'],
                       models       = ['HPYTM'],
                       train_params = {'b':[500],
                                       'n':[10],
                                       'i':[10],
                                       'K':[10,50,100],
                                       'O':[3],
                                       'e':[0]},
                       exp_name     = 'emnlp.hpytm_token')
    return jobs

def hpytm_token_final():
    jobs = create_jobs(corpora      = ['nips','bnc'],
                       models       = ['HPYTM'],
                       train_params = {'b':[500],
                                       'n':[10],
                                       'i':[10],
                                       'K':[100],
                                       'O':[4,-1],
                                       'e':[0]},
                       exp_name     = 'emnlp.hpytm_token_final')
    return jobs

def hpylm():
    jobs = create_jobs(corpora      = ['brown','nips','bnc'],
                       models       = ['HPYTM'],
                       train_params = {'b':[100],
                                       'n':[10],
                                       'i':[10],
                                       'K':[1],
                                       'O':[3,4,-1],
                                       'e':[0]},
                       exp_name     = 'emnlp.hpylm')
    return jobs

if __name__ == '__main__':
    os.environ['LD_LIBRARY_PATH'] = './build/src:' + os.environ['LD_LIBRARY_PATH']

    if len(sys.argv) != 2:
        print "usage", sys.argv[0], "expname (table_based_effect, hpytm_token, hpytm, dhpytm, chpytm rescaling bnc_final nips_final rescaling_final)" 
        exit()

    expname = sys.argv[1]
    if expname == 'table_based_effect':
        jobs = table_based_effect()
    elif expname == 'hpytm_token':
        jobs = hpytm_token()
    elif expname == 'hpytm':
        jobs = hpytm()
    elif expname == 'dhpytm':
        jobs = dhpytm()
    elif expname == 'chpytm':
        jobs = chpytm()
    elif expname == 'rescaling':
        jobs = rescaling()
    elif expname == 'bnc_final':
        jobs = bnc_final()
    elif expname == 'rescaling_final':
        jobs = rescaling_final()
    elif expname == 'nips_final':
        jobs = nips_final()
    elif expname == 'bnc_light_O3':
        jobs = bnc_light_O3()
    elif expname == 'hpytm_final':
        jobs = hpytm_final()
    elif expname == 'hpytm_token':
        jobs = hpytm_token()
    elif expname == 'hpytm_token_final':
        jobs = hpytm_token_final()
    elif expname == 'hpylm':
        jobs = hpylm()
    
    for j in jobs:
        j.start()
    for j in jobs:
        j.join()
