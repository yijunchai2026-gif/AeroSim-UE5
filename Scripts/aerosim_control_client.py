#!/usr/bin/env python3
"""Exercise actual MCP stdio initialize/tools/list/tools/call; useful for local replay evidence."""
import json,subprocess,sys
from pathlib import Path
class Client:
    def __init__(self):
        self.process=subprocess.Popen([sys.executable,str(Path(__file__).with_name('aerosim_control_mcp.py'))],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,bufsize=1)
        self.sequence=0
        self.rpc('initialize',{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'aerosim-regression','version':'1'}})
        self.process.stdin.write(json.dumps({'jsonrpc':'2.0','method':'notifications/initialized'})+'\n');self.process.stdin.flush()
        self.tools=self.rpc('tools/list',{})['tools']
    def rpc(self,method,params):
        self.sequence+=1
        self.process.stdin.write(json.dumps({'jsonrpc':'2.0','id':self.sequence,'method':method,'params':params})+'\n');self.process.stdin.flush()
        reply=json.loads(self.process.stdout.readline())
        if reply.get('id')!=self.sequence or 'error' in reply:raise RuntimeError(reply)
        return reply['result']
    def call(self,name,**arguments):
        result=self.rpc('tools/call',{'name':name,'arguments':arguments})
        data=json.loads(result['content'][0]['text']) if not result.get('isError') else result
        if result.get('isError'):raise RuntimeError(data)
        return data
    def close(self):
        self.process.stdin.close();self.process.wait(timeout=5);self.process.stdout.close()
if __name__=='__main__':
    client=Client()
    try:print(json.dumps(client.call(sys.argv[1] if len(sys.argv)>1 else 'aerosim_inspect',**(json.loads(sys.argv[2]) if len(sys.argv)>2 else {})),indent=2))
    finally:client.close()
