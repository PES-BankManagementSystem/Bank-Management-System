// S-43: isolated TC-03, TC-08, TC-18 and responsive evidence.
// NODE_PATH must point to scripts/browser-check/node_modules from the tested tree.
const puppeteer = require('puppeteer-core');
const fs = require('fs');
const path = require('path');
const [name, executablePath, base, out, testCase] = process.argv.slice(2);
async function main() {
  fs.mkdirSync(out, {recursive:true});
  const browser = await puppeteer.launch({browser:name, executablePath, headless:true});
  const page = await browser.newPage();
  const result = {case:testCase, browser:name, version:await browser.version(), checks:[], scriptErrors:[]};
  page.on('pageerror', e => result.scriptErrors.push(String(e)));
  const check = (ok, detail) => { result.checks.push({ok,detail}); if(!ok) throw Error(detail); };
  const set = async (selector,value) => page.$eval(selector,(el,v) => {el.value=v;el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));},value);
  async function login(user) {
    await set('#login-username',user); await set('#login-password','Password123!');
    await page.click('#login-submit');
    await page.waitForFunction(() => !document.querySelector('#app-user').hidden);
  }
  try {
    await page.setViewport({width:1280,height:900});
    await page.goto(base,{waitUntil:'networkidle0'});
    if(testCase==='TC-03') {
      let token;
      page.on('response',async res => {if(res.url().endsWith('/auth/login') && res.status()===200) token=(await res.json()).accessToken;});
      await login('officer1');
      await page.waitForFunction(() => document.querySelector('#role-home-heading').textContent.includes('Bank Officer'));
      const response = page.waitForResponse(r => r.url().endsWith('/auth/logout'));
      await page.click('#app-sign-out');
      check((await response).status()===204,'UI logout request returns 204');
      await page.waitForFunction(() => document.querySelector('#app-user').hidden && document.querySelector('#login-form').getClientRects().length>0);
      check(true,'Sign-in form visible after logout');
      const status=await page.evaluate(async token => (await fetch('/dashboard',{headers:{Authorization:'Bearer '+token}})).status,token);
      check(status===401,'Old browser session token receives 401');
    } else if(testCase==='TC-08') {
      await login('officer1'); let dialogText='';
      page.on('dialog',async dialog => {dialogText=dialog.message(); await dialog.accept();});
      await set('#account-number','ACC001'); await set('#amount','5000.00');
      await page.click('#submit-button');
      await page.waitForFunction(() => document.querySelector('#result').textContent.includes('succeeded'));
      check(dialogText.includes('ACC001') && /5[,.]?000/.test(dialogText),'Confirmation identifies ACC001 and INR 5,000');
      const text=await page.$eval('#result',el=>el.textContent);
      check(/15[,.]?000/.test(text),'Post-success view shows INR 15,000');
      result.dialog=dialogText; result.success=text;
    } else if(testCase==='TC-18') {
      await login('customer1');
      await set('#statement-account-number','ACC001'); await set('#statement-from','2026-09-01'); await set('#statement-to','2026-10-18');
      await page.click('#statement-submit');
      await page.waitForFunction(() => !document.querySelector('#statement-output').hidden);
      await page.evaluate(() => {const original=URL.createObjectURL;window.validationBlobs=[];URL.createObjectURL=function(blob){window.validationBlobs.push(blob);return original.call(this,blob);};});
      await page.click('#statement-download');
      const csv=await page.evaluate(async()=>window.validationBlobs.length ? window.validationBlobs[0].text() : '');
      fs.writeFileSync(path.join(out,'statement.csv'),csv);
      check(csv.includes('ACC001') && csv.includes('Opening balance') && csv.includes('Closing balance'),'Download button generates formatted account CSV with balances');
      check((await page.$$('#statement-entries tr')).length===12,'Twelve fixture transactions displayed');
      result.csvBytes=Buffer.byteLength(csv);
    } else {
      result.layouts=[];
      for(const user of ['officer1','customer1','admin']) {
        await login(user);
        await page.waitForFunction(() => document.querySelector('#role-home-heading').textContent.includes('dashboard'));
        await page.waitForFunction(() => !document.querySelector('#dashboard-status').textContent.includes('Loading'));
        if(user==='admin') await page.waitForSelector('#audit-list table');
        for(const width of [1280,768,390]) {
          await page.setViewport({width,height:900});
          await new Promise(r=>setTimeout(r,200));
          const layout=await page.evaluate(()=>({viewport:innerWidth,documentWidth:document.documentElement.scrollWidth,heading:document.querySelector('#role-home-heading').textContent.trim(),nav:[...document.querySelectorAll('#app-nav a')].filter(el=>el.getClientRects().length).map(el=>el.textContent.trim()),cards:[...document.querySelectorAll('.card')].filter(el=>el.getClientRects().length).map(el=>({id:el.id,width:Math.round(el.getBoundingClientRect().width)})),tableRegions:[...document.querySelectorAll('#dashboard-details, #audit-list')].filter(el=>el.querySelector('table') && el.getClientRects().length).map(el=>{el.scrollLeft=el.scrollWidth;const reachable=el.scrollWidth<=el.clientWidth || el.scrollLeft>0;el.scrollLeft=0;return {id:el.id,clientWidth:el.clientWidth,scrollWidth:el.scrollWidth,reachable,keyboardFocusable:el.tabIndex===0,label:el.getAttribute('aria-label'),overflowX:getComputedStyle(el).overflowX};})}));
          result.layouts.push({user,width,...layout});
          result.checks.push({ok:layout.documentWidth<=width,detail:`${user} at ${width}px has no page horizontal overflow`});
          for(const region of layout.tableRegions) {
            result.checks.push({ok:region.reachable && region.keyboardFocusable && Boolean(region.label) && region.overflowX==='auto',detail:`${user} at ${width}px: ${region.id} is labeled, focusable and its columns can be reached`});
          }
          await page.screenshot({path:path.join(out,`${user}-${width}.png`),fullPage:true});
        }
        await page.click('#app-sign-out');
        await page.waitForFunction(()=>document.querySelector('#app-user').hidden);
      }
    }
    await page.screenshot({path:path.join(out,'final.png'),fullPage:true});
    check(result.scriptErrors.length===0,'No uncaught JavaScript errors');
    result.status=result.checks.every(item=>item.ok)?'PASS':'FAIL';
  } catch(e) {result.status='FAIL';result.error=String(e);}
  finally {await browser.close();fs.writeFileSync(path.join(out,'result.json'),JSON.stringify(result,null,2));}
  console.log(JSON.stringify({case:testCase,browser:name,status:result.status,error:result.error}));
  process.exitCode=result.status==='PASS'?0:1;
}
main().catch(e=>{console.error(e);process.exitCode=2;});
