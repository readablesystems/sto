#!/usr/bin/env python3

def get_cache_name():
  '''Get the name of the cache file.'''
  return 'aws.json'

def get_labels():
  '''Produce a list of labels and corresponding tag names, etc..'''
  labels = {
    'tpcc-osto': {
      'name': 'TPC-C OCC',
      'script': 'cd ~/sto && run/run_tpcc_occ.sh',
    },
    'tpcc-tsto': {
      'name': 'TPC-C TicToc',
      'script': 'cd ~/sto && run/run_tpcc_tictoc.sh',
    },
    'tpcc-msto': {
      'name': 'TPC-C MVCC',
      'script': 'cd ~/sto && run/run_tpcc_mvcc.sh',
    },
    'rubis-osto': {
      'name': 'RUBiS OCC',
      'script': 'cd ~/sto && run/run_rubis_occ.sh',
    },
    'rubis-tsto': {
      'name': 'RUBiS TicToc',
      'script': 'cd ~/sto && run/run_rubis_tictoc.sh',
    },
    'rubis-msto': {
      'name': 'RUBiS MVCC',
      'script': 'cd ~/sto && run/run_rubis_mvcc.sh',
    },
    'wiki-osto': {
      'name': 'Wikipedia OCC',
      'script': 'cd ~/sto && run/run_wiki_occ.sh',
    },
    'wiki-tsto': {
      'name': 'Wikipedia TicToc',
      'script': 'cd ~/sto && run/run_wiki_tictoc.sh',
    },
    'wiki-msto': {
      'name': 'Wikipedia MVCC',
      'script': 'cd ~/sto && run/run_wiki_mvcc.sh',
    },
    'ycsb-a': {
      'name': 'YCSB-A',
      'script': 'cd ~/sto && run/run_ycsb_a.sh',
    },
    'ycsb-b': {
      'name': 'YCSB-B',
      'script': 'cd ~/sto && run/run_ycsb_b.sh',
    },
    'ycsb-c': {
      'name': 'YCSB-C',
      'script': 'cd ~/sto && run/run_ycsb_c.sh',
    },
    'adapting-2sp': {
      'name': 'Adapting 2sp',
      'script': 'cd ~/sto && run/run_adapting_2sp.sh',
    },
    'adapting-3sp': {
      'name': 'Adapting 3sp',
      'script': 'cd ~/sto && run/run_adapting_3sp.sh',
    },
    'adapting-4sp': {
      'name': 'Adapting 4sp',
      'script': 'cd ~/sto && run/run_adapting_4sp.sh',
    },
    'like': {
      'name': 'LIKE',
      'script': 'cd ~/sto && run/run_like.sh',
    },
    'tpcc-osto-comp': {
      'name': 'TPC-C OCC ATS Compare',
      'script': 'cd ~/sto && git checkout ats-overhead && git reset --hard origin/ats-overhead && run/run_tpcc_occ.sh',
    },
    'tpcc-tsto-comp': {
      'name': 'TPC-C TicToc ATS Compare',
      'script': 'cd ~/sto && git checkout ats-overhead && git reset --hard origin/ats-overhead && run/run_tpcc_tictoc.sh',
    },
    'tpcc-msto-comp': {
      'name': 'TPC-C MVCC ATS Compare',
      'script': 'cd ~/sto && git checkout ats-overhead && git reset --hard origin/ats-overhead && run/run_tpcc_mvcc.sh',
    },
    'ycsb-a-comp': {
      'name': 'YCSB-A ATS Compare',
      'script': 'cd ~/sto && git checkout ats-overhead && git reset --hard origin/ats-overhead && run/run_ycsb_a.sh',
    },
    'ycsb-b-comp': {
      'name': 'YCSB-B ATS Compare',
      'script': 'cd ~/sto && git checkout ats-overhead && git reset --hard origin/ats-overhead && run/run_ycsb_b.sh',
    },
    'ycsb-c-comp': {
      'name': 'YCSB-C ATS Compare',
      'script': 'cd ~/sto && git checkout ats-overhead && git reset --hard origin/ats-overhead && run/run_ycsb_c.sh',
    },
  }
  return labels

def get_reader(fobj):
  '''Reads from a file-like object in perpetuity.'''
  for line in fobj:
    print(line, end='')

def launch(**kwargs):
  '''Launch the experiment machines.'''
  import json
  import os

  import boto3

  if os.path.exists(get_cache_name()):
    inp = None
    while inp not in ('yes', 'no'):
      inp = input(f'Cache file {get_cache_name()} already exists. Overwrite (yes/no)? << ')
    if inp != 'yes':
      return

  labels = get_labels()

  ec2 = boto3.client('ec2')

  response = ec2.run_instances(
    LaunchTemplate={
      'LaunchTemplateName': 'STODAT',
    },
    MinCount=len(labels),
    MaxCount=len(labels),
  )

  mappings = {}
  for instance, (label, settings) in zip(response.get('Instances', []), labels.items()):
    name = settings.get('name', label)
    instanceid = instance.get('InstanceId', None)
    ec2.create_tags(
      Resources=[instanceid],
      Tags=[
        {
          'Key': 'Name',
          'Value': name,
        }
      ],
    )
    mappings[label] = {
      'name': name,
      'instanceid': instanceid,
    }

  with open(get_cache_name(), 'w') as fout:
    json.dump(mappings, fout)

  print(f'Success! {len(mappings)} resources written to {get_cache_name()}.')

def peek(key, quiet=False, **kwargs):
  '''Optionally peek through a cached instance.'''
  import json
  import os
  import threading
  import time

  import boto3
  import paramiko

  if not key:
    return 'Invalid SSH private key.'

  with open(get_cache_name(), 'r') as fin:
    mappings = json.load(fin)

  ec2 = boto3.resource('ec2')

  sshkey = paramiko.RSAKey.from_private_key_file(key)
  labels = get_labels()
  while True:
    try:
      machines = input('Peek into which machine? << ')
      if machines == 'all':
        machines = mappings.keys()
      elif machines not in labels:
        print(f'Invalid machine: {machines}')
        print(f'Available machines: {tuple(mappings.keys())}')
        continue
      else:
        machines = [machines]
      for label in machines:
        cache = mappings[label]
        name = cache['name']
        instanceid = cache['instanceid']
        instance = ec2.Instance(instanceid)

        if instance.state['Name'] == 'stopped':
          inp = None
          while inp not in ('yes', 'no'):
            inp = input(f'{label} ({instanceid}) is stopped. Restart it? (yes/no) << ')
          if inp != 'yes':
            continue
          instance.start()
          print(f'Waiting for {label} ({instanceid}) to start', end='', flush=True)
          while instance.state['Name'] != 'running':
            time.sleep(1)
            print('.', end='', flush=True)
            instance.reload()
          print()
        elif instance.state['Name'] != 'running':
          print(f'{label} ({instanceid}) is current {instance.state["Name"]}.')
          continue

        print(f'Peeking into instance {name}: {instance.public_dns_name}')

        #ssh = paramiko.SSHClient()
        #ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        #ssh.connect(hostname=instance.public_dns_name, username='ubuntu', pkey=sshkey)
        #channel = ssh.invoke_shell()
        #ssh.close()
        os.system(f'ssh -i {key} -o "StrictHostKeyChecking no" ubuntu@{instance.public_dns_name}')
    except EOFError:
      break
    except Exception as e:
      print(e)
  print()

def run(key, quiet=False, **kwargs):
  '''Run through each cached instance.'''
  import json
  import threading

  import boto3
  import paramiko

  if not key:
    return 'Invalid SSH private key.'

  with open(get_cache_name(), 'r') as fin:
    mappings = json.load(fin)

  ec2 = boto3.resource('ec2')

  sshkey = paramiko.RSAKey.from_private_key_file(key)
  labels = get_labels()
  for label, cache in mappings.items():
    name = cache['name']
    instanceid = cache['instanceid']
    instance = ec2.Instance(instanceid)

    print(f'\r\033[2KRunning instance {name}: {instance.public_dns_name}', end='')

    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(hostname=instance.public_dns_name, username='ubuntu', pkey=sshkey)
    stdin, stdout, stderr = ssh.exec_command('bash')
    if not quiet:
      print()
      t_stdout = threading.Thread(target=get_reader, args=(stdout,))
      t_stderr = threading.Thread(target=get_reader, args=(stderr,))
      t_stdout.start()
      t_stderr.start()

    stdin.write(
        'tmux has-session -t script || tmux new-session -s script -d\n')
    stdin.flush()

    #stdin.write('tmux send -t script.0 cd ~/sto && make clean && rm -rf results ENTER\n')
    #stdin.write('tmux send -t script.0 git pull --rebase ENTER\n')
    script = labels[label].get('script', 'ls')
    stdin.write(f'tmux send -t script.0 "cd ~/sto && make clean && rm -rf results && git fetch origin && git reset --hard origin/ats; {script}" ENTER\n')
    stdin.flush()

    if not quiet:
      # Total of up to 4 seconds of waiting
      t_stdout.join(2)
      t_stderr.join(2)

    stdin.close()
    stdout.close()
    stderr.close()
    ssh.close()
  print()

def main():
  '''Main execution starting point.'''
  import argparse

  functions = {
    'launch': launch,
    'peek': peek,
    'run': run,
  }

  parser = argparse.ArgumentParser(description='STO experiment manager')
  parser.add_argument('function', action='store',
                      choices=functions.keys(), type=str,
                      help='Experiment functionality.')
  parser.add_argument('-k', '--key', action='store', required=False, type=str,
                      help='SSH private key file.')
  parser.add_argument('-q', '--quiet', action='store_true', default=False,
                      help='Shhh don\' wake the baby computer...')

  args = parser.parse_args()

  error = functions[args.function](**dict(args._get_kwargs()))  # None is success
  if error:
    print(f'\033[1;31m{error}\033[0m')
    parser.print_help()

if __name__ == '__main__':
    main()
