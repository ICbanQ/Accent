#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
	int i;
	int len;
	char sz[512];
	char *str;

	if (argc <= 1)
		return 0;


	if (!strcmp(argv[1], "init"))
	{
		//tty �ʱ�ȭ
		system("stty -F /dev/ttyAMA0 9600");
	}
	else
	{
		//�Ű����� ���̸� Ȯ��
		str = argv[1];

		len = strlen(str);

		for (i = 0; i < len; i++)
		{
			//�� �ܾ �и��Ͽ� echo�� ����

			sprintf(sz, "echo \"%c\" > /dev/ttyAMA0", str[i]);
			system(sz);
		}
	}

	return 0;
}