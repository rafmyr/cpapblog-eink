// SPDX-License-Identifier: GPL-3.0-or-later
// Minimal mock renderer for x-dc boards: board file -> static html (resolves one dc-import of a template)
const fs=require('fs'),path=require('path');
class DCLogic{constructor(p){this.props=p||{};}}
function getScript(src){const m=src.match(/<script type="text\/x-dc"[^>]*>([\s\S]*?)<\/script>/);return m?m[1]:'';}
function vals(src,props){const code=getScript(src);const C=new Function('DCLogic',code+'\nreturn Component;')(DCLogic);return new C(props).renderVals();}
function get(ctx,expr){expr=expr.trim();if(expr==='false')return false;if(expr==='true')return true;try{return new Function(...Object.keys(ctx),'return ('+expr+');')(...Object.values(ctx));}catch(e){return '';}}
function render(tpl,ctx){
  let out='',i=0;
  const re=/<(sc-if|sc-for)\b([^>]*)>/g;
  while(true){re.lastIndex=i;const m=re.exec(tpl);if(!m){out+=subst(tpl.slice(i),ctx);break;}
    out+=subst(tpl.slice(i,m.index),ctx);
    const tag=m[1];let depth=1,j=re.lastIndex;const t=new RegExp('<(/?)'+tag+'\\b[^>]*>','g');t.lastIndex=j;let mm;
    while(depth&&(mm=t.exec(tpl))){depth+=mm[1]?-1:1;if(!depth)break;}
    const inner=tpl.slice(j,mm.index);i=t.lastIndex;
    const attrs={};m[2].replace(/([\w-]+)="([^"]*)"/g,(_,k,v)=>attrs[k]=v);
    if(tag==='sc-if'){const v=get(ctx,attrs.value.replace(/^\{\{|\}\}$/g,''));if(v)out+=render(inner,ctx);}
    else{const list=get(ctx,attrs.list.replace(/^\{\{|\}\}$/g,''))||[];for(const it of list){out+=render(inner,Object.assign({},ctx,{[attrs.as]:it}));}}
  }
  return out;}
function subst(s,ctx){return s.replace(/\{\{([\s\S]*?)\}\}/g,(_,e)=>{const v=get(ctx,e);return v==null?'':String(v);});}
const [board,out]=process.argv.slice(2);const dir=path.dirname(board);
let b=fs.readFileSync(board,'utf8');const bv=vals(b,{});
let body=b.match(/<x-dc>([\s\S]*?)<\/x-dc>/)[1];
body=body.replace(/<dc-import name="(\w+)" cfg="\{\{cfg\}\}"[^>]*><\/dc-import>/,(_,n)=>{const t=fs.readFileSync(path.join(dir,n+'.dc.html'),'utf8');const tv=vals(t,{cfg:bv.cfg});const tb=t.match(/<x-dc>([\s\S]*?)<\/x-dc>/)[1].replace(/<helmet>[\s\S]*?<\/helmet>/,'');return render(tb,tv);});
body=render(body,bv).replace(/<\/?helmet>/g,'');
fs.writeFileSync(out,'<!doctype html><html><head><meta charset="utf-8"></head><body style="margin:0">'+body+'</body></html>');
