from __future__ import print_function
import httplib2
import os
from pprint import pprint

from apiclient import discovery
import oauth2client
from oauth2client import client
from oauth2client import tools
from collections import defaultdict

try:
    import argparse
    flags = argparse.ArgumentParser(parents=[tools.argparser]).parse_args()
except ImportError:
    flags = None

BENCHMARK_FILE = "cds_benchmarks_stats.txt"
INIT_SIZES = [1000, 10000, 50000, 100000, 150000]
NTHREADS = [1, 2, 4, 8, 12, 16, 20]
TESTS = ["PQRandSingleOps:R","PQRandSingleOps:D","PQPushPop:R",
            "PQPushPop:D", "PQPushOnly:R", "PQPushOnly:D", 
            "Q:PushPop","Q:RandSingleOps",
            "HM1M:F34,I33,E33","HM1M:F90,I5,E5",
            "HM125K:F34,I33,E33","HM125K:F90,I5,E5",
        ]

# sheet id, row index, column index
TEST_COORDS = {
    "PQRandSingleOps:R" : {
        1000: (0, 5, 7),
        10000: (0, 14, 7),
        50000: (0, 23, 7),
        100000: (0, 32, 7),
        150000: (0, 41, 7)
    },
    "PQRandSingleOps:D" : {
        1000: (0, 5, 1),
        10000: (0, 14, 1),
        50000: (0, 23, 1),
        100000: (0, 32, 1),
        150000: (0, 41, 1)
    },
    "PQPushPop:R" : {
        1000: (1077482895, 4, 1),
        10000: (1077482895, 5, 1),
        50000: (1077482895, 6, 1),
        100000: (1077482895, 7, 1),
        150000: (1077482895, 8, 1)
    },
    "PQPushPop:D" : {
        1000: (1077482895, 10, 1),
        10000: (1077482895, 11, 1),
        50000: (1077482895, 12, 1),
        100000: (1077482895, 13, 1),
        150000: (1077482895, 14, 1)
    },
    "PQPushOnly:R" : {
        1000: (444106569, 29, 7),
        10000: (444106569, 38, 7),
        50000: (444106569, 48, 7),
        100000: (444106569, 57, 7),
        150000: (444106569, 65, 7)
    }, 
    "PQPushOnly:D" : {
        1000: (444106569, 29, 1),
        10000: (444106569, 38, 1),
        50000: (444106569, 48, 1),
        100000: (444106569, 57, 1),
        150000: (444106569, 65, 1)
    }, 
    "Q:PushPop" : {
        1000: (780112938, 3, 1),
        10000: (780112938, 4, 1),
        50000: (780112938, 5, 1),
        100000: (780112938, 6, 1),
        150000: (780112938, 7, 1)
    }, 
    "Q:RandSingleOps" : {
        1000: (720124793, 40, 1),
        10000: (720124793, 49, 1),
        50000: (720124793, 58, 1),
        100000: (720124793, 67, 1),
        150000: (720124793, 76, 1)
    }, 
    "HM1M:F34,I33,E33" : {
        1000: (577880275, 40, 1),
        10000: (577880275, 49, 1),
        50000: (577880275, 58, 1),
        100000: (577880275, 67, 1),
        150000: (577880275, 76, 1)
    }, 
    "HM1M:F90,I5,E5" : {
        1000: (660987677, 40, 1),
        10000: (660987677, 49, 1),
        50000: (660987677, 58, 1),
        100000: (660987677, 67, 1),
        150000: (660987677, 76, 1)
    }, 
    "HM200K:F34,I33,E33" : {
        1000: (1080807431, 40, 1),
        10000: (1080807431, 49, 1),
        50000: (1080807431, 58, 1),
        100000: (1080807431, 67, 1),
        150000: (1080807431, 76, 1)
    }, 
    "HM200K:F90,I5,E5" : {
        1000: (91535127, 40, 1),
        10000: (91535127, 49, 1),
        50000: (91535127, 58, 1),
        100000: (91535127, 67, 1),
        150000: (91535127, 76, 1)
    }, 
}

# If modifying these scopes, delete your previously saved credentials
# at ~/.credentials/sheets.googleapis.com-python-quickstart.json
SCOPES = 'https://www.googleapis.com/auth/spreadsheets'
CLIENT_SECRET_FILE = 'client_secret_cds_benchmarks.json'
APPLICATION_NAME = 'CDS Benchmarks'

def get_credentials():
    """Gets valid user credentials from storage.

    If nothing has been stored, or if the stored credentials are invalid,
    the OAuth2 flow is completed to obtain the new credentials.

    Returns:
        Credentials, the obtained credential.
    """
    home_dir = os.path.expanduser('~')
    credential_dir = os.path.join(home_dir, '.credentials')
    if not os.path.exists(credential_dir):
        os.makedirs(credential_dir)
    credential_path = os.path.join(credential_dir,
                                   'sheets.googleapis.com-python-quickstart.json')

    store = oauth2client.file.Storage(credential_path)
    credentials = store.get()
    if not credentials or credentials.invalid:
        flow = client.flow_from_clientsecrets(CLIENT_SECRET_FILE, SCOPES)
        flow.user_agent = APPLICATION_NAME
        if flags:
            credentials = tools.run_flow(flow, store, flags)
        else: # Needed only for compatibility with Python 2.6
            credentials = tools.run(flow, store)
        print('Storing credentials to ' + credential_path)
    return credentials

def main():
    '''
    Parse the data file cds_benchmarks_stats.txt
    We should end up with a dictionary with the following format:
        
        test_name {
            size1: [
                [data structure data for 1 threads],
                [data structure data for 2 threads],
                ...
            ]
            size2: [
                [data structure data for 1 threads],
                [data structure data for 2 threads],
                ...
            ]
        }

    A dictionary is hardcoded above for the coordinates of where the data should go
    on the Google Sheets with the following format: 
    
        test_name {
            size1: (sheetid, rowid, colid),
            size2: (sheetid, rowid, colid),
            ...
        }

    '''
    
    tests = defaultdict(dict)
    size_index = 0
    test = ""
    with open(BENCHMARK_FILE, "r") as f:
        for line in f.read().splitlines():
            if (line == ""):
                size_index += 1
                size_index %= len(INIT_SIZES)
            elif line in TESTS:
                test = line    
                size_index = 0
            else:
                if INIT_SIZES[size_index] not in tests[test]:
                    tests[test][INIT_SIZES[size_index]] = []
                tests[test][INIT_SIZES[size_index]].append(
                        [float(x) for x in line.split(",")[:-1]])

    # Spreadsheet stuff
    credentials = get_credentials()
    http = credentials.authorize(httplib2.Http())
    discoveryUrl = ('https://sheets.googleapis.com/$discovery/rest?'
                    'version=v4')
    service = discovery.build('sheets', 'v4', http=http,
                              discoveryServiceUrl=discoveryUrl)

    spreadsheetId = '15fHBqqtfJRCueLseBjBoBai-YIInzZFhCrCBiwzW-b4'

    # Set up request
    requests = []
    for test, size_dict in TEST_COORDS.iteritems():
        if test not in tests: 
            continue
        for size, (s,r,c) in size_dict.iteritems():
            if size not in tests[test]: 
                continue

            # get the row data for the specified test/size combination
            rows = []
            for row in tests[test][size]:
                row_values = []
                for d in row:
                    row_values.append({'userEnteredValue' : {'numberValue': d}})
                rows.append({ 'values' : row_values })
           
            # append the row data to the request
            requests.append({
                'updateCells': {
                    'start' : {
                        'sheetId' : s,
                        'rowIndex' : r,
                        'columnIndex' : c,
                    },
                    'rows' : rows,
                    'fields' : 'userEnteredValue'
                }
            })

    
    batchUpdateRequest = {'requests': requests}
    result = service.spreadsheets().batchUpdate(
        spreadsheetId=spreadsheetId, body=batchUpdateRequest).execute()

    with open('cds_benchmarks_stats_history.txt', 'a') as f:
        f.write("---------------REQUESTS----------------\n")
        pprint(requests, stream=f)
        f.write("---------------RESPONSE----------------\n")
        pprint(result, stream=f)

if __name__ == '__main__':
    main()
