1. 这个工具有什么用
------------
此脚本的的产生是为了代替Haproxy检测mysql可用性功能的.
Haproxy默认仅是用户名和密码连接一下mysql这的端口,这种检测方法过于简单,不适用以复杂的业务需求.

此工具以守护进程的方式运行在MySQL服务器上,TCP监听在5000端口(可调整),通过tcp或是http连接来触发对mysql的检测.

此脚本默认检测mysql可用性的方法是执行"show processlist"查询,然后以执行是否成功来判定.
检测失败,会返回一个http status code为 500 内容到client端,然后会把相应的失败信息写进log,检测成功,
则返回http status code 200 的内容到clinet同时记录mysql check 所花费的时间(查询时间)
当然"show processlist"也不一定能把mysql所有的问题检测出来,但是这个查询是可以调整.

2. 为什么不用别人写好的脚本
------------
是的,这种端口监听,守护进程,连接mysql,处理http请求的脚本其它语言perl,python,ruby,php.
还有linux 自来的xinetd加上shell脚本,都可以实现,而且github上也有现成的.为什么还要用C来实现一个.
首先是这些语方都需要安装一些复杂的扩展包,比如mysql连接器,perl需要安装perl-DBI,python的MySQldb,ruby也是需安装相应的gem包.
守护进程什么的,就更需要其它的扩展包了,有时候MySQL服务器上是不能上网或是不能随便安装软件包的.
xinet.d+shell 的问题是经常响应超时,而且没有太多的日志说明问题在什么地方.
所以现成的脚本,可以用,但是并不好用.


3. 如何用好此脚本
------------
* 调整用户名
    打开 mysqlhealth.c 文件的 32 行,即可看到相应的项,直接变更,重新编绎就可以.
  ```

    /* DATABASE  connection info */
    #define DBSERVER   "localhost"
    #define DBUSER     "用户名"
    #define DBPASS     "密码"
    #define DBNAME     "information_schema"
    #define QUERY      "show processlist"
  ```
* 编绎和平台问题
  编绎:
    静态编绎 仅限centos5和centos6和RPM包版MySQL-Client5.1和5.5版本,其它的版本没有测试过,编绎方法为 make static
    动态编绎 osx 平台只能动态编绎直接 make 
    平台: osx 和unix like

* 守护进程的安全
  进程唯一性: 同一时间只能运行一个进程.如果重复运行会直接退出,不给任何提示
  进程常驻保证: 已然是后台daemon进程,然后通过inittab的respawn时实进程守护来保证此进程不会被停掉
