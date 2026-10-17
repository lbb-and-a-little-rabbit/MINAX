import {state,t} from './state';
const messages={
 OPTIMIZER_UNAVAILABLE:['求解服务暂不可用，请稍后重试。','Optimization is temporarily unavailable.'],
 INVALID_BOUNDS:['请检查变量名，以及上下界和初始值；下界须小于上界。','Check variable names, bounds and initial values. Lower bounds must be smaller than upper bounds.'],
 INVALID_CONSTRAINTS:['请检查约束，每行应包含一个 =、<= 或 >=。','Each constraint must contain one =, <= or >=.'],
 UNSUPPORTED_CONSTRAINT:['求解暂支持 =、<=、>=，不支持严格不等式或不等于。','Use =, <= or >=; strict inequalities and != are not supported.'],
 OPTIMIZATION_TIMEOUT:['本次求解超时，请缩小变量范围或简化约束后重试。','Search timed out. Narrow the bounds or simplify constraints.'],
 OPTIMIZATION_FAILED:['求解未完成，请检查输入或稍后重试。','Optimization failed. Check your input or retry later.'],
 SESSION_EXPIRED:['登录已过期，请重新登录或继续以游客使用。','Your session expired. Sign in or continue as a guest.'],
 REQUEST_REJECTED:['操作未完成，请刷新页面后重试。','Refresh the page and try again.'],
 INVALID_CREDENTIAL_FORMAT:['用户名需为 3–32 位字母、数字或下划线；密码需为 8–72 个英文字符或数字。','Use a 3–32 character username and an 8–72 byte password.'],
 INVALID_CREDENTIALS:['用户名或密码不正确。','Incorrect username or password.'],USERNAME_TAKEN:['该用户名已被使用，请换一个。','This username is already taken.'],
 TOO_MANY_REQUESTS:['操作过于频繁，请稍后再试。','Too many attempts. Please try again later.'],
 CALCULATOR_UNAVAILABLE:['计算服务暂不可用，请稍后重试。','Calculation is temporarily unavailable. Try again later.'],
 SERVICE_UNAVAILABLE:['服务暂不可用，请稍后重试。','The service is temporarily unavailable. Try again later.'],
 INVALID_EXPRESSION:['表达式格式不正确，请检查运算符、括号和逗号。','Check operators, parentheses and commas in your expression.'],
 INVALID_VARIABLES:['请检查变量名是否重复，以及变量值是否为有效数字。','Check for duplicate names and invalid variable values.'],
 DIVISION_BY_ZERO:['除数不能为 0，请检查表达式和变量值。','Cannot divide by zero. Check your expression and values.'],
 UNKNOWN_VARIABLE:['表达式中有未定义的变量，请在变量表中补充。','Add the missing variable to the variable list.'],
 UNKNOWN_FUNCTION:['该函数暂不支持，请查看使用说明中的函数列表。','This function is not supported. See the function list in Help.'],
 INVALID_ARGUMENTS:['函数参数数量不正确，请查看使用说明。','Incorrect number of function arguments. See Help.'],
 INTEGER_BOUNDS_REQUIRED:['求和或连乘的上下界必须是整数。','Sum and product bounds must be integers.'],
 INVALID_DOMAIN:['当前数值不在函数允许范围内，或结果过大。','A value is outside the allowed range, or the result is too large.'],
 CALCULATION_TIMEOUT:['计算时间过长，请缩小求和范围后重试。','Calculation took too long. Reduce the range and retry.'],
 CALCULATION_LIMIT:['计算量过大，请缩小范围。','Reduce the calculation range and retry.'],
 BUSY:['当前计算较多，请稍后重试。','Calculations are busy. Try again shortly.'],
 NOT_AVAILABLE:['该功能尚未开放。','This feature is not available yet.'],
 EXPRESSION_TOO_LONG:['表达式过长，请简化后重试。','The expression is too long. Simplify it and retry.'],
 EXPRESSION_TOO_DEEP:['括号嵌套过多，请简化表达式。','Too many nested parentheses. Simplify the expression.'],
 INVALID_REQUEST:['输入内容不正确，请检查后重试。','Check your input and try again.'],
 CALCULATION_FAILED:['计算未完成，请检查表达式或稍后重试。','Calculation failed. Check the expression or try again later.']
};
export function errorText(code){return t(...(messages[code] || ['操作未完成，请稍后重试。','The operation failed. Please try again.']));}
export async function api(path,{method='GET',data,timeoutMs}={}){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),timeoutMs ?? window.MINAX_CONFIG?.requestTimeoutMs ?? 15000);
 try{
  const headers={};
  if(method!=='GET'){headers['Content-Type']='application/json';headers['X-CSRF-Token']=state.csrf;}
  const response=await fetch((window.MINAX_CONFIG?.apiBase ?? '/api').replace(/\/$/,'')+path,{method,headers,credentials:'include',body:data===undefined?undefined:JSON.stringify(data),signal:controller.signal});
  let result;try{result=await response.json();}catch{throw new Error(t('暂时无法完成操作，请确认服务已启动后重试。','Unable to complete the operation. Try again when the service is available.'));}
  if(!response.ok){const e=new Error(errorText(result.error?.code));e.code=result.error?.code;if(e.code==='SESSION_EXPIRED'){state.ready=false;}throw e;}
  return result;
 }catch(e){if(e.name==='AbortError')throw new Error(t('等待时间过长，请重试。','The request timed out. Please retry.'));if(e instanceof TypeError)throw new Error(t('暂时无法连接，请稍后重试。','Unable to connect. Please try again.'));throw e;}
 finally{clearTimeout(timer);}
}
export async function initialize(){state.sessionError='';try{const r=await api('/session');state.user=r.user;state.csrf=r.csrf;state.ready=true;}catch(e){state.sessionError=e.message;state.ready=false;}}
