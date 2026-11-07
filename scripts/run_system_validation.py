"""S-43 independent fresh-seed API validation. No credentials are written to evidence.

Usage: python scripts/run_system_validation.py SOURCE SERVER OUTPUT BUILD_SHA
Browser/manual cases are deliberately reported separately, never inferred from API checks.
"""
import datetime as dt
import hashlib
import json
import pathlib
import sqlite3
import subprocess
import sys
import time
import urllib.request
import urllib.error

SOURCE, SERVER, OUT = map(lambda x: pathlib.Path(x).resolve(), sys.argv[1:4])
SHA = sys.argv[4]
OUT.mkdir(parents=True, exist_ok=True)
BASE = 'http://127.0.0.1:18943'
current = None
db = None

def check(value, message):
    if not value:
        raise AssertionError(message)
    current['assertions'].append(message)

def sql(query, args=()):
    with sqlite3.connect(db) as conn:
        rows = conn.execute(query, args).fetchall()
        return rows

def request(method, path, token=None, body=None, status=200):
    headers = {'Content-Type': 'application/json'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    req = urllib.request.Request(BASE + path, data=None if body is None else json.dumps(body).encode(), headers=headers, method=method)
    start = time.perf_counter()
    try:
        res = urllib.request.urlopen(req, timeout=10)
    except urllib.error.HTTPError as err:
        res = err
    raw = res.read()
    elapsed = (time.perf_counter() - start) * 1000
    data = json.loads(raw) if raw else {}
    current['requests'].append({'method': method, 'path': path, 'status': res.code, 'ms': round(elapsed, 3), 'code': data.get('code')})
    check(res.code == status, f'{method} {path}: expected {status}, got {res.code} ({data.get("code", "")})')
    return data

def login(user='officer1', password='Password123!'):
    return request('POST', '/auth/login', body={'username': user, 'password': password})['accessToken']

def tx(token, amount='1.00', kind='deposit', account='ACC001', status=201):
    data=request('POST', '/transactions', token, {'type': kind, 'account_number': account, 'amount': amount}, status)
    if status==201:
        stored=sql('select balance from accounts where account_number=?',(account,))[0][0]
        check(f'{stored//100}.{stored%100:02}'==data['balance'],'Persisted account balance matches transaction response')
    return data

def transfer(token, amount='1.00', source='ACC001', target='ACC004', status=201):
    data=request('POST', '/transfers', token, {'from_account_number': source, 'to_account_number': target, 'amount': amount}, status)
    if status==201:
        for account,key in ((source,'from_balance'),(target,'to_balance')):
            stored=sql('select balance from accounts where account_number=?',(account,))[0][0]
            check(f'{stored//100}.{stored%100:02}'==data[key],'Persisted transfer balance matches response: '+account)
    return data

def balances():
    return sql('select account_number,balance from accounts order by account_id')

def state():
    return balances(), sql('select count(*) from transactions')

def setup(case):
    if case == 'TC-05':
        sql("insert into account_types(type_name,min_balance,interest_rate,daily_withdrawal_limit) values('Zero',0,0,100000)")
    if case == 'TC-07':
        sql("insert into accounts(account_number,customer_id,type_id,status,balance) values('ZERO001',1,1,'active',0)")
    if case in ('TC-09',):
        sql('update account_types set daily_withdrawal_limit=2500000 where type_id=1')
        sql("insert into transactions(account_id,operator_id,type,amount,resulting_balance,created_at) values(1,2,'withdrawal',2500000,5000000,datetime('now','-1 day'))")
    if case == 'TC-10':
        sql('update accounts set balance=1000000 where account_id=1')
        sql('update accounts set balance=500000 where account_id=4')
    if case == 'TC-18':
        for i in range(12):
            sql("insert into transactions(account_id,operator_id,type,amount,resulting_balance,created_at) values(1,2,'deposit',100,?,?)", (5000100+i*100, f'2026-10-{i+1:02} 10:00:00'))
        sql('update accounts set balance=5001200 where account_id=1')
    if case in ('TC-19', 'TC-20'):
        with sqlite3.connect(db) as conn:
            conn.executemany("insert into accounts(account_number,customer_id,type_id,status,balance) values(?,1,1,'active',5000000)", [(f'PERF{i:05}',) for i in range(3000)])

def execute(case):
    if case == 'TC-01':
        for name, role in [('admin','administrator'),('officer1','bank_officer'),('customer1','customer')]:
            request('POST','/auth/login',body={'username':name,'password':'wrong'},status=401)
            data = request('POST','/auth/login',body={'username':name,'password':'Password123!'})
            check(len(data['accessToken']) == 64 and data['role'] == role, '64-character token and correct role: '+name)
            check(not ({'password','password_hash','passwordHash','temporaryPassword'} & set(data)), 'No password or hash in login response')
        check(all(x[0] == 0 for x in sql('select failed_attempts from users')), 'Failed attempt counters zero')
        check(all(x[0].startswith('$argon2id$') for x in sql('select password_hash from users')), 'Stored hashes use Argon2id')
        return
    if case in ('TC-02','TC-02B'):
        for i in range(3):
            data=request('POST','/auth/login',body={'username':'customer2','password':'wrong'},status=401)
            check(data.get('code') == 'ACCOUNT_LOCKED' if i == 2 else 'Invalid' in data.get('message',''), 'Lockout response attempt '+str(i+1))
        check(sql('select status,failed_attempts from users where user_id=5') == [('locked',3)], 'Locked after three failures')
        if case == 'TC-02B':
            request('PATCH','/users/5/unlock',login('admin'),{})
            check(sql('select status,failed_attempts from users where user_id=5') == [('active',0)], 'Unlock resets status and failures')
            login('customer2')
        return
    officer=login()
    if case == 'TC-03':
        request('POST','/auth/logout',officer,{},204)
        request('GET','/dashboard',officer,status=401)
        current['status']='PARTIAL'
        current['gap']='API 204/401 confirmed; UI redirect requires separate browser evidence.'
    elif case == 'TC-04':
        body={'name':'Validation User','dateOfBirth':'1995-04-10','address':'1 Test Lane','identificationReference':'S43-1','email':'s43@example.com'}
        data=request('POST','/customers',officer,body,400)
        check('phone' in json.dumps(data), 'Missing phone has field error')
        body['phone']='9876543210'
        request('POST','/customers',officer,body,201)
        check(sql("select count(distinct customer_code) from customers")[0][0] == 4, 'Unique customer code created')
    elif case == 'TC-04B':
        customer=login('customer1'); admin=login('admin'); before=sql('select count(*) from transactions')
        for token in (officer,customer):
            request('PUT','/customers/CUS-00000001',token,{'status':'inactive'},403)
        for _ in range(2): request('PUT','/customers/CUS-00000001',admin,{'status':'inactive'})
        request('GET','/dashboard',customer,status=401)
        request('POST','/auth/login',body={'username':'customer1','password':'Password123!'},status=401)
        request('POST','/accounts',officer,{'customerId':'CUS-00000001','accountType':1,'initialDeposit':'1000.00'},409)
        check(sql('select count(*) from transactions') == before and len(balances()) == 5, 'Account and transaction records retained')
    elif case == 'TC-05':
        body={'customerId':'CUS-00000001','accountType':1,'initialDeposit':'999.00'}; before=state()
        data=request('POST','/accounts',officer,body,422)
        check(data['code']=='BELOW_MINIMUM_BALANCE' and state()==before,'Below-minimum rejection has no account or ledger mutation')
        body['initialDeposit']='1000.00'; request('POST','/accounts',officer,body,201)
        body.update(accountType=3,initialDeposit='0.00'); request('POST','/accounts',officer,body,201)
        check(sql('select amount from transactions order by transaction_id desc limit 2') == [(0,),(100000,)], 'Zero and exact-minimum opening deposits recorded')
    elif case in ('TC-06','TC-09'):
        before=state()
        if case == 'TC-06':
            tx(officer,'49001.00','withdraw',status=422)
            tx(officer,'50001.00','withdraw',status=422)
        else:
            data=tx(officer,'30000.00','withdraw',status=422)
            check(data['code']=='DAILY_LIMIT_EXCEEDED','UTC daily-limit rejection')
        check(state()==before,'Rejected withdrawals preserve balances and ledger')
        tx(officer,'1.00' if case=='TC-06' else '25000.00','withdraw')
        if case=='TC-09':
            check(len(sql("select distinct date(created_at) from transactions where type='withdrawal' and account_id=1 and created_at >= date('now','-1 day')"))==2,'Synthetic prior-day and current-day withdrawals counted separately (no clock rollover simulated)')
            before=state(); tx(officer,'1.00','withdraw',status=422)
            check(state()==before,'Current UTC day total enforces limit after valid withdrawal')
    elif case == 'TC-07':
        request('PATCH','/accounts/ACC001/status',officer,{'action':'close'},409)
        for action in ('freeze','freeze'):
            request('PATCH','/accounts/ACC001/status',officer,{'action':action})
        tx(officer,status=422); tx(officer,kind='withdraw',status=422); transfer(officer,status=422)
        request('PATCH','/accounts/ACC001/status',officer,{'action':'unfreeze'})
        for action in ('freeze','close','close'):
            request('PATCH','/accounts/ZERO001/status',officer,{'action':action})
        tx(officer,account='ZERO001',status=422); tx(officer,kind='withdraw',account='ZERO001',status=422); transfer(officer,source='ZERO001',status=422)
        check(sql("select status from accounts where account_number='ZERO001'")==[('closed',)],'Closed account retained')
    elif case == 'TC-09B':
        admin=login('admin'); request('POST','/interest/run',admin,{'period':'2026-10'},201); before=state()
        request('POST','/interest/run',admin,{'period':'2026-10'},409)
        check(state()==before,'Duplicate interest leaves balances and ledger unchanged')
    elif case == 'TC-10':
        data=transfer(officer,'3000.00')
        check(data['from_balance']=='7000.00' and data['to_balance']=='8000.00','Expected transfer balances')
        check(len(sql('select * from transactions where transfer_ref=?',(data['transfer_reference'],)))==2,'Two entries share transfer reference')
        before=state()
        sql("CREATE TRIGGER validation_credit_failure BEFORE INSERT ON transactions WHEN NEW.type='transfer_credit' BEGIN SELECT RAISE(ABORT,'validation forced credit failure'); END")
        transfer(officer,status=500)
        check(state()==before,'Forced failure after debit rolls back both balances and ledger')
        sql('DROP TRIGGER validation_credit_failure')
        current['fault_method']='SQLite credit-insert failure after debit; process termination was not used.'
    elif case == 'TC-10B':
        before=state(); transfer(officer,target='ACC001',status=400); transfer(officer,'999999.00',status=422)
        check(state()==before,'Invalid transfers preserve balances and ledger')
    elif case == 'TC-11':
        customer=login('customer1')
        request('GET','/accounts/ACC001/balance',customer)
        request('GET','/accounts/ACC001/transactions?from=2026-01-01&to=2026-12-31',customer)
        request('GET','/accounts/ACC002/balance',customer,status=403)
        request('GET','/accounts/ACC002/transactions',customer,status=403)
        check(bool(sql("select * from audit_log where action='VIEW_TRANSACTIONS_DENIED'")), 'Denied history access audited')
    elif case in ('TC-12','TC-13'):
        admin=login('admin'); data=request('POST','/staff',admin,{'username':'officer_new'},201)
        token=login('officer_new',data['login']['temporaryPassword'])
        request('POST','/auth/change-password',token,{'newPassword':'NewPassword123!'})
        token=login('officer_new','NewPassword123!'); uid=data['userId']
        check(sql('select password_hash from users where user_id=?',(uid,))[0][0].startswith('$argon2id$'),'New officer has Argon2id hash')
        if case=='TC-13':
            tx(token); request('PATCH',f'/staff/{uid}',officer,{'status':'inactive'},403)
            request('PATCH',f'/staff/{uid}',admin,{'status':'inactive'})
            request('GET','/dashboard',token,status=401)
            check(sql('select operator_id from transactions order by transaction_id desc limit 1')==[(uid,)],'Historical operator identity retained')
    elif case == 'TC-14':
        data=[]
        for user in ('admin','officer1','customer1'):
            data.append(request('GET','/dashboard',login(user)))
            check(current['requests'][-1]['ms']<3000,'Dashboard under 3 seconds: '+user)
        check(len({json.dumps(x,sort_keys=True) for x in data})==3,'Distinct role dashboard payloads')
        check(data[0]['counts']=={'customers':3,'accounts':5,'staff':2},'Administrator counts match fresh seed')
        check(data[1]['transactionsToday']['count']==0 and 'counts' not in data[1], 'Officer receives only own current-day transactions and no system counts')
        check({x['accountNumber'] for x in data[2]['accounts']}=={'ACC001','ACC004'} and 'counts' not in data[2], 'Customer dashboard contains only owned accounts')
        current['dashboards']=data
    elif case == 'TC-16':
        tx(officer); tx(officer,kind='withdraw'); transfer(officer)
        rows=sql('select action,details,user_id,timestamp from audit_log where log_id>8')
        current['audit_rows']=rows
        check(len(rows)>=3 and all(x[2]==2 and x[3] for x in rows),'Financial operations have operator and timestamp in audit')
        admin=login('admin')
        for method in ('PUT','DELETE'): request(method,'/audit/1',admin,{},405)
    elif case in ('TC-17','TC-19'):
        customer=login('customer1')
        for _ in range(30 if case=='TC-19' else 1):
            if case=='TC-19':
                data=request('GET','/dashboard',customer)
                check(current['requests'][-1]['ms']<3000,'Account list under 3 seconds')
                expected={row[0] for row in sql('select account_number from accounts where customer_id=1')}
                check({x['accountNumber'] for x in data['accounts']}==expected,'Performance dashboard returns exactly authorized accounts')
            ids=[]; entries=[]
            for offset in (0,2):
                data=request('GET',f'/accounts/ACC001/transactions?from=2026-01-16&to=2026-01-18&limit=2&offset={offset}',customer)
                check(current['requests'][-1]['ms']<3000,'Filtered history under 3 seconds')
                entries.extend(data['transactions']); ids.extend(x['transactionId'] for x in data['transactions'])
            check(len(ids)==3 and len(set(ids))==3,'Pagination has three unique in-range entries')
            check([x['dateTime'] for x in entries]==sorted((x['dateTime'] for x in entries),reverse=True),'Newest first ordering')
            check(all('2026-01-16'<=x['dateTime'][:10]<='2026-01-18' for x in entries),'Inclusive date bounds')
            check(all(x['amount'].startswith('-') for x in entries if x['type'] in ('withdrawal','transfer_debit')),'Debit amounts signed negative')
    elif case == 'TC-18':
        data=request('GET','/accounts/ACC001/statement?from=2026-09-01&to=2026-10-18',login('customer1'))
        entries=data['transactions']; check(len(entries)==12,'Statement contains twelve fixture entries')
        check([x['created_at'] for x in entries]==sorted(x['created_at'] for x in entries),'Statement chronological')
        check(data['opening_balance']=='50000.00' and data['closing_balance']=='50012.00','Statement opening/closing match retained ledger fixture')
        current['status']='PARTIAL'; current['gap']='API statement checked; browser CSV export remains to be verified.'
    elif case == 'TC-20':
        check(len(balances())==3005,'Performance fixture has 3,005 accounts')
        for operation in ('deposit','withdraw','transfer'):
            samples=[]
            for i in range(33):
                before=state()
                data=transfer(officer) if operation=='transfer' else tx(officer,kind=operation)
                ms=current['requests'][-1]['ms']
                check(ms<2000,operation+' persisted response under 2 seconds')
                check(sql('select count(*) from transactions')[0][0]==before[1][0][0]+(2 if operation=='transfer' else 1),'Persisted ledger count matches success')
                check(state()[0]!=before[0],'Persisted balances changed')
                if i>=3: samples.append(ms)
            current.setdefault('performance',{})[operation]={'samples':30,'mean_ms':sum(samples)/30,'max_ms':max(samples)}

cases=['TC-01','TC-02','TC-02B','TC-03','TC-04','TC-04B','TC-05','TC-06','TC-07','TC-09','TC-09B','TC-10','TC-10B','TC-11','TC-12','TC-13','TC-14','TC-16','TC-17','TC-18','TC-19','TC-20']
if len(sys.argv)>5:
    requested=sys.argv[5:]
    if any(case not in cases for case in requested): raise ValueError('Unknown case')
    cases=requested
results=[]
for case in cases:
    current={'case':case,'build_sha':SHA,'started_utc':dt.datetime.now(dt.timezone.utc).isoformat(),'status':'PASS','assertions':[],'requests':[],'fresh_seed':True}
    db=OUT/(case+'.db')
    if db.exists():
        raise RuntimeError('Refusing to reuse existing database: '+str(db))
    with sqlite3.connect(db) as conn:
        for name in ('schema.sql','seed.sql'): conn.executescript((SOURCE/'db'/name).read_text())
    current['seed_counts']={table:sql('select count(*) from '+table)[0][0] for table in ('users','customers','accounts','transactions')}
    setup(case)
    log=open(OUT/(case+'.server.log'),'w')
    process=subprocess.Popen([str(SERVER),'--port','18943','--db',str(db),'--frontend',str(SOURCE/'frontend')],stdout=log,stderr=log)
    try:
        for attempt in range(100):
            try:
                urllib.request.urlopen(BASE+'/health',timeout=.2).close(); break
            except (OSError,urllib.error.URLError): time.sleep(.1)
        else: raise RuntimeError('Server did not become healthy')
        execute(case)
    except Exception as err:
        current['status']='FAIL'; current['error']=str(err)
    finally:
        process.terminate(); process.wait(timeout=10); log.close()
    current['database_sha256']=hashlib.sha256(db.read_bytes()).hexdigest()
    (OUT/(case+'.json')).write_text(json.dumps(current,indent=2))
    results.append({k:current[k] for k in ('case','status')})
    print(case,current['status'],current.get('error',''),flush=True)
(OUT/'summary.json').write_text(json.dumps({'build_sha':SHA,'results':results},indent=2))
sys.exit(any(result['status']=='FAIL' for result in results))
