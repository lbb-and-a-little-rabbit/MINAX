import { createApp } from 'vue';
import { createRouter, createWebHashHistory } from 'vue-router';
import App from './App.vue';
import EvaluatePage from './pages/EvaluatePage.vue';
import OptimizePage from './pages/OptimizePage.vue';
import HistoryPage from './pages/HistoryPage.vue';
import GuidePage from './pages/GuidePage.vue';
import SettingsPage from './pages/SettingsPage.vue';
import AccountPage from './pages/AccountPage.vue';
import './style.css';
const router=createRouter({history:createWebHashHistory(),routes:[
 {path:'/',redirect:'/evaluate'},
 {path:'/evaluate',component:EvaluatePage,meta:{title:'计算'}},
 {path:'/optimize',component:OptimizePage,meta:{title:'最值求解'}},
 {path:'/history',component:HistoryPage,meta:{title:'计算记录'}},
 {path:'/guide',component:GuidePage,meta:{title:'使用说明'}},
 {path:'/settings',component:SettingsPage,meta:{title:'设置'}},
 {path:'/account',component:AccountPage,meta:{title:'账户'}},
 {path:'/:pathMatch(.*)*',redirect:'/evaluate'}
]});
router.afterEach(to=>{document.title=to.meta.title+' · MINAX';});
createApp(App).use(router).mount('#app');
