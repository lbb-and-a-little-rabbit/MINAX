"""Run from project root: python -m unittest backend.test_api -v.
Set MINAX_TEST_NATIVE to test the real calculator (otherwise native cases skip).
"""
import os
import sqlite3
import tempfile
import unittest
from pathlib import Path
from backend.server import create_app

HEADERS = {'X-Minax-Client': 'web'}
class ApiTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.db = str(Path(self.temp.name) / 'test.sqlite3')
        self.native = os.environ.get('MINAX_TEST_NATIVE', '')
        self.app = create_app({'DB_PATH':self.db,'RATE_LIMIT':False,
            'NATIVE_EXE':self.native or str(Path(self.temp.name)/'missing.exe')})
        self.client = self.app.test_client()
    def tearDown(self):
        self.temp.cleanup()
    def post(self, path, data=None, client=None):
        return (client or self.client).post('/api'+path,json=data or {},headers=HEADERS)
    def guest(self, client=None):
        response = self.post('/auth/guest', client=client)
        self.assertEqual(response.status_code,200)
        return response
    def test_auth_required(self):
        self.assertEqual(self.client.get('/api/history').status_code,401)
        self.assertEqual(self.post('/evaluate').status_code,401)
    def test_origin_and_request_header(self):
        self.assertEqual(self.client.post('/api/auth/guest',json={}).status_code,403)
        self.assertEqual(self.client.post('/api/auth/guest',json={},headers={**HEADERS,'Origin':'https://evil.example'}).status_code,403)
    def test_guest_cookie_logout(self):
        cookie = self.guest().headers['Set-Cookie']
        self.assertIn('HttpOnly',cookie);self.assertIn('SameSite=Lax',cookie)
        self.assertTrue(self.client.get('/api/auth/me').json['user']['guest'])
        self.post('/auth/logout')
        self.assertIsNone(self.client.get('/api/auth/me').json['user'])
    def test_register_login_and_password_storage(self):
        credentials={'username':'alice','password':'a-long-password'}
        self.assertEqual(self.post('/auth/register',credentials).status_code,200)
        self.assertEqual(self.post('/auth/register',credentials).status_code,409)
        with sqlite3.connect(self.db) as con:
            hashed,salt=con.execute('SELECT password,salt FROM users').fetchone()
        self.assertNotEqual(hashed,credentials['password']);self.assertEqual(len(salt),32)
        self.post('/auth/logout')
        self.assertEqual(self.post('/auth/login',{'username':'alice','password':'bad-password'}).status_code,401)
        self.assertEqual(self.post('/auth/login',credentials).status_code,200)
    def test_pending_features(self):
        self.guest()
        self.assertEqual(self.post('/optimize').status_code,501)
        self.assertFalse(self.client.get('/api/health').json['minaParser'])
    def test_payload_validation(self):
        self.guest()
        cases=[{'expression':'x','variables':[{'name':'x','value':True}]},
            {'expression':'x','variables':[{'name':'bad name','value':1}]},
            {'expression':'x','variables':[{'name':'x','value':float('inf')}]},
            {'expression':'x','variables':[{'name':'x','value':1},{'name':'x','value':2}]},
            {'expression':'','variables':[]}]
        for payload in cases:
            with self.subTest(payload=payload):self.assertEqual(self.post('/evaluate',payload).status_code,400)
    def test_missing_engine(self):
        app=create_app({'DB_PATH':self.db,'RATE_LIMIT':False,'NATIVE_EXE':str(Path(self.temp.name)/'missing.exe')})
        c=app.test_client();self.guest(c)
        self.assertEqual(self.post('/evaluate',{'expression':'1 + 2','variables':[]},c).status_code,503)
    def test_static_build(self):
        response=self.client.get('/')
        self.assertEqual(response.status_code,200);response.close()
        config=self.client.get('/config.js')
        self.assertEqual(config.status_code,200);self.assertEqual(config.headers['Cache-Control'],'no-store');config.close()
        self.assertEqual(self.client.get('/../backend/server.py').status_code,404)
    def test_native_evaluation_and_isolated_history(self):
        if not self.native:self.skipTest('Set MINAX_TEST_NATIVE to a real calculator executable')
        self.guest()
        cases=[('sum ( i , 1 , 3 , i * x ) + pow ( y , 2 )',[{'name':'x','value':2},{'name':'y','value':3}],21),
            ('sum ( i , 1 , 3 , prod ( j , 1 , i , j ) )',[],9),
            ('sqrt ( pow ( x , 2 ) + pow ( y , 2 ) )',[{'name':'x','value':3},{'name':'y','value':4}],5)]
        for expression,variables,expected in cases:
            response=self.post('/evaluate',{'expression':expression,'variables':variables})
            self.assertEqual(response.status_code,200,response.json)
            self.assertEqual(response.json['value'],expected)
        self.assertEqual(len(self.client.get('/api/history').json['items']),3)
        other=self.app.test_client();self.guest(other)
        self.assertEqual(other.get('/api/history').json['items'],[])
        self.client.delete('/api/history',json={},headers=HEADERS)
        self.assertEqual(self.client.get('/api/history').json['items'],[])
    def test_native_errors(self):
        if not self.native:self.skipTest('Set MINAX_TEST_NATIVE to a real calculator executable')
        self.guest()
        for expression in ['1 / 0','sqrt ( - 1 )','unknown + 1','1 +','sum ( i , 1.5 , 3 , i )']:
            with self.subTest(expression=expression):
                response=self.post('/evaluate',{'expression':expression,'variables':[]})
                self.assertEqual(response.status_code,422,response.json)

if __name__ == '__main__':unittest.main()
