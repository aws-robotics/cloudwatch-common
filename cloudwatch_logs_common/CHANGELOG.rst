^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package cloudwatch_logs_common
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
1.0.2 (2019-06-21)
------------------
* Adding old release to change log
* Update package.xml for 1.0.2 release
  Signed-off-by: Ryan Newell <ryanewel@amazon.com>
* Increase test timeout to fix flakiness (`#21 <https://github.com/aws-robotics/cloudwatch-common/issues/21>`_)
  - Increase the test timeout to 5000ms because sometimes
  SendLogsToCloudWatch isn't being called fast enough and so the tests
  fail.
* Fixes issue with DescribeLogStreams being called to get the next token
  - Call DescribeLogStreams only one on startup
  - Implements a state machine to manage the core run loop instead of checking if the log stream exists every tick.
* Update success logs' severity to DEBUG in cloudwatch_facade.cpp
* Release 1.0.1 (`#14 <https://github.com/aws-robotics/cloudwatch-common/issues/14>`_)
  * Release 1.0.1
  * 1.0.1
* adding unit tests for cloudwatch facade
* Merge pull request `#4 <https://github.com/aws-robotics/cloudwatch-common/issues/4>`_ from juanrh/improve-coverage-cloudwatch_logger
  Improve coverage cloudwatch logger
* Make LogManagerFactory mockeable
* Make cloudwatch_logs_common shared lib to use it in other libs
* Merge pull request `#1 <https://github.com/aws-robotics/cloudwatch-common/issues/1>`_ from xabxx/master
  [Bug Fix] Resolved false-positive error log messages
* Resolved false-positive error log messages
* Contributors: AAlon, Abby Xu, Nick Burek, Ross Desmond, Ryan Newell, Tim Robinson, Yuan "Forrest" Yu, hortala, ryanewel

1.0.0 (2019-03-20)
------------------

1.0.1 (2019-03-20)
------------------
* adding unit tests for cloudwatch facade
* Merge pull request `#4 <https://github.com/aws-robotics/cloudwatch-common/issues/4>`_ from juanrh/improve-coverage-cloudwatch_logger
  Improve coverage cloudwatch logger
* Make LogManagerFactory mockeable
* Make cloudwatch_logs_common shared lib to use it in other libs
* Merge pull request `#1 <https://github.com/aws-robotics/cloudwatch-common/issues/1>`_ from xabxx/master
  [Bug Fix] Resolved false-positive error log messages
* Resolved false-positive error log messages
* Contributors: Abby Xu, Ross Desmond, Ryan Newell, Yuan "Forrest" Yu, hortala
