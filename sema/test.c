// 测试程序
// 连着运行两次，第一次运行不会阻塞，第二次运行会阻塞。
// sudo ./test
// sudo ./test
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
main()
{
  int fd;

  printf("before open\n ");
  fd = open("/dev/test", O_RDWR); // 原子变量  0
  if (fd < 0)
  {
    perror("open fail \n");
    return;
  }
  printf("open ok ,sleep......\n ");
  sleep(20);
  printf("wake up from sleep!\n ");
  close(fd); // 加为1
}