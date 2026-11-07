"""Run S-43 UI cases and TC-22 with independent seeded databases.

Usage: python scripts/run_system_validation_browsers.py SOURCE SERVER OUTPUT SHA CHROME FIREFOX
"""
import datetime
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import time
import urllib.request

source, server, out = [Path(x).resolve() for x in sys.argv[1:4]]
sha, chrome, firefox = sys.argv[4:7]
chrome, firefox = str(Path(chrome).resolve()), str(Path(firefox).resolve())
out.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env['NODE_PATH'] = str(source/'scripts/browser-check/node_modules')
env['CHROME_PATH'], env['FIREFOX_PATH'] = chrome, firefox
runner = Path(__file__).with_name('system_validation_browser.js').resolve()
summary=[]
for name, executable in [('chrome',chrome),('firefox',firefox)]:
    for case in (['TC-03','TC-08','TC-18','TC-22-core','TC-22-responsive'] if name=='chrome' else ['TC-22-core','TC-22-responsive']):
        if len(sys.argv)>7 and case not in sys.argv[7:]: continue
        target=out/(name+'-'+case); target.mkdir()
        db=target/'test.db'
        with sqlite3.connect(db) as conn:
            for file in ('schema.sql','seed.sql'): conn.executescript((source/'db'/file).read_text())
            if case=='TC-08': conn.execute('update accounts set balance=1000000 where account_id=1')
            if case=='TC-18':
                for i in range(12): conn.execute("insert into transactions(account_id,operator_id,type,amount,resulting_balance,created_at) values(1,2,'deposit',100,?,?)",(5000100+i*100,f'2026-10-{i+1:02} 10:00:00'))
                conn.execute('update accounts set balance=5001200 where account_id=1')
        log=open(target/'server.log','w')
        proc=subprocess.Popen([str(server),'--port','18944','--db',str(db),'--frontend',str(source/'frontend')],stdout=log,stderr=log)
        try:
            for _ in range(100):
                try: urllib.request.urlopen('http://127.0.0.1:18944/health',timeout=.2).close(); break
                except OSError: time.sleep(.1)
            if case=='TC-22-core':
                cmd=['node',str(source/'scripts/browser-check/walkthrough.js'),name,'http://127.0.0.1:18944/',str(target/'screenshots')]
            else:
                cmd=['node',str(runner),name,executable,'http://127.0.0.1:18944/',str(target),case]
            run=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=300)
            (target/'stdout.json').write_text(run.stdout)
            (target/'stderr.txt').write_text(run.stderr)
            record={'browser':name,'case':case,'build_sha':sha,'fresh_seed':True,'exit_code':run.returncode,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
            if case=='TC-08':
                with sqlite3.connect(db) as conn:
                    record['balance']=conn.execute('select balance from accounts where account_id=1').fetchone()[0]
                    record['last_transaction']=conn.execute('select type,amount,resulting_balance,operator_id from transactions order by transaction_id desc limit 1').fetchone()
            print(name,case,run.returncode,flush=True)
        except Exception as err:
            record={'browser':name,'case':case,'build_sha':sha,'error':str(err),'exit_code':2}
            print(record,flush=True)
        finally:
            proc.terminate();proc.wait(timeout=10);log.close()
        record['database_sha256']=hashlib.sha256(db.read_bytes()).hexdigest()
        (target/'metadata.json').write_text(json.dumps(record,indent=2));summary.append(record)
(out/'summary.json').write_text(json.dumps(summary,indent=2))
sys.exit(any(result.get('exit_code',1)!=0 for result in summary))
