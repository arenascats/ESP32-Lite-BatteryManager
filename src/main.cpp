
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#define BATINPUT 32       //电池输入引脚
#define ADCFILTER_SIZE 20 //ADC容器
volatile int MenuMin;
volatile int MenuMax;
volatile int menu;
volatile int firstLine;
volatile int offset; //Time x pos offset
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
int8_t timeZone = 8;
const PROGMEM char *ntpServer = "ntp1.aliyun.com";

unsigned int adc_value[ADCFILTER_SIZE];

//------读取ADC的数值的滤波器，获取电池的电压
int filter()
{
  int temp;
  int time = 0;
  for (int i = 0; i < ADCFILTER_SIZE; i++)
  {
    adc_value[i] = analogRead(BATINPUT);
  }
  while (1)
  {
    time = 0;
    for (int i = 0; i < ADCFILTER_SIZE; i++)
    {
      if (adc_value[i] > adc_value[i + 1])
      {
        temp = adc_value[i + 1];
        adc_value[i + 1] = adc_value[i];
        adc_value[i] = temp;
        time += 1;
      }
    }
    if (time == 0)
    {
      break;
    }
  }
  Serial.println(adc_value[ADCFILTER_SIZE / 2]);
  return adc_value[ADCFILTER_SIZE / 2];
}

double Vol_last = 0;
void CurrentTime()
{
  double Vol = (double)filter() * 19.6 / 4096.0;
  if (abs(Vol_last - Vol) > 0.05)
  {
    Vol_last = Vol;
  }
  u8g2.setFont(u8g2_font_timR14_tf);
  u8g2.setFontPosTop();
  u8g2.setCursor(3 + offset, firstLine);
  u8g2.print(NTP.getTimeHour24());
  u8g2.setCursor(30 + offset, firstLine);
  u8g2.print(":");
  u8g2.setCursor(43 + offset, firstLine);
  u8g2.print(NTP.getTimeMinute());
  u8g2.setCursor(72 + offset, firstLine);
  u8g2.print(":");
  u8g2.setCursor(85 + offset, firstLine);
  u8g2.print(NTP.getTimeSecond());
  //配置文字
  u8g2.setFont(u8g2_font_timR08_tf);
  u8g2.setFontPosTop();
  // 32脚位的电池输入电压采集，以及【V】图标
  u8g2.setCursor(28, 22);
  u8g2.print(Vol_last);
  u8g2.setCursor(28 + 4 * 5, 22);
  u8g2.print("V");
  u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
  //电池图标
  u8g2.drawGlyph(3, 2 + 2 * 8, 90);
  //时钟外框
  u8g2.drawFrame(2, 2, 109, 20);
}

int lastRecX = 0;
int point[128] = {0};
int auto_min = 0;
int auto_max = 0;

//-----------自动测量最大值最小值
void measure()
{
  int temp;
  int time = 0;
  int sample = 128;
  int adc_value[128];
  //采集128个数据
  for (int i = 0; i < sample; i++)
  {
    adc_value[i] = analogRead(BATINPUT);
    if (adc_value[i] > 4095)
    {
      adc_value[i] = 4095;
    }
  }
  //排序从小到大
  while (1)
  {
    time = 0;
    for (int i = 0; i < sample; i++)
    {
      if (adc_value[i] > adc_value[i + 1])
      {
        temp = adc_value[i + 1];
        adc_value[i + 1] = adc_value[i];
        adc_value[i] = temp;
        time += 1;
      }
    }
    if (time == 0)
    {
      break;
    }
  }
  auto_min = adc_value[2];
  auto_max = adc_value[127]; //最大值取95%那部分
  Serial.println("AutoMeasure:");
  Serial.println(auto_max);
  Serial.println(auto_min);

  delay(2000);
}
double scale = 1.0;

//改变图形绘制比例，配合绘制函数，实现垂直轴缩放
void scaleCHG()
{

  scale += 0.5;
  if (scale == 3)
  {
    scale = 0.5;
  }
}
int fillflag = 0;
void BatteryCurve() // TODO  电池曲线的绘制
{
  int lineY = 2;
  int temp = ((auto_max + 5) - (analogRead(BATINPUT)) * (auto_max / auto_min));

  if (fillflag < 127) //一开始先填充空的数组
  {
    point[fillflag] = temp;
    fillflag += 1;
  }
  else //填充完成，朝向左侧移动，数值向左传递
  {
    for (int i = 1; i <= 127; i++)
    {

      // u8g2.drawPixel(i,point[i]);
      point[i-1] = point[i];

      // u8g2.drawLine(lastRecX,0,lastRecX,31);
    }
    point[127] = temp;//只实时更新最后一个点
  }
  //坐标打印函数
  for (int i = 1; i <= 127; i++)
  {

    // u8g2.drawPixel(i,point[i]);
    u8g2.drawLine(i - 1, (31 - point[i - 1]) * scale, i, (31 - point[i]) * scale);
    // u8g2.drawLine(lastRecX,0,lastRecX,31);
  }
  u8g2.setFont(u8g2_font_timR08_tf);
  //在右上角打印电压
  u8g2.setCursor(u8g2.getDisplayWidth() - 6 * 5, lineY);
  u8g2.print((double)filter() * 19.6 / 4096.0);
  //打印现在的绘图比例
  u8g2.setCursor(2, lineY);
  u8g2.print(scale);
  u8g2.setCursor(u8g2.getDisplayWidth() - 2 * 5, lineY);
  u8g2.print("V");
  u8g2.setFont(u8g2_font_open_iconic_all_2x_t);

  lastRecX += 1;

  delay(10);
}
void TimeCounter_Topic()
{
  u8g2.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2.setFontPosTop();
  u8g2.setCursor(3, 6);
  u8g2.print("计时器");
  u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
  u8g2.drawGlyph(58, -10 + 2 * 8, 238);
}
void lowBatterySleep()
{
  double Vol = analogRead(BATINPUT) * 19.6 / 4096.0;
  Serial.print("Battery:");
  Serial.println(Vol);
  if (Vol < 3.3)
  {
    u8g2.clear();
    u8g2.setFont(u8g2_font_wqy16_t_gb2312a);
    u8g2.setFontPosTop();
    u8g2.setCursor(3, 6);
    u8g2.print("电量低请充电");
    u8g2.sendBuffer();

    delay(1500);
    u8g2.clear();
    u8g2.sleepOn();
    esp_deep_sleep_start();
  }
}
void TimeCounter()
{

  u8g2.setFont(u8g2_font_timR14_tf);
  u8g2.setFontPosTop();
  u8g2.setCursor(3 + offset, firstLine);
  u8g2.print(NTP.getTimeHour24());
  u8g2.setCursor(30 + offset, firstLine);
  u8g2.print(":");
  u8g2.setCursor(43 + offset, firstLine);
  u8g2.print(NTP.getTimeMinute());
  u8g2.setCursor(72 + offset, firstLine);
  u8g2.print(":");
  u8g2.setCursor(85 + offset, firstLine);
  u8g2.print(NTP.getTimeSecond());
  u8g2.drawFrame(2, 2, 109, 20);
}

void ReverseTimeCounter_Topic()
{
  u8g2.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2.setFontPosTop();
  u8g2.setCursor(3, 6);
  u8g2.print("倒计时");
  u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
  u8g2.drawGlyph(58, -10 + 2 * 8, 263);
}

void CurrentTime_Topic()
{
  u8g2.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2.setFontPosTop();
  u8g2.setCursor(3, 6);
  u8g2.print("CurrentTime");
  u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
  u8g2.drawGlyph(58, -10 + 2 * 8, 175);
}

void FuncSelect()
{
  switch (menu)
  {
  case 0:
    CurrentTime();
    break;
  case 1:
    ReverseTimeCounter_Topic();
    break;
  case 2:
    TimeCounter_Topic();
    break;
  case 3:
    BatteryCurve();
    break;
  default:
    break;
  }
}
void FuncSet()
{

  switch (menu)
  {
  case 0:

    break;
  case 1:

    break;
  case 2:

    break;
  case 3:
    scaleCHG();
    break;
  default:
    break;
  }
}

void PWROut()
{
  u8g2.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2.setFontPosTop();
  u8g2.setCursor(3, 6);
  u8g2.print("CurrentTime");
  u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
  u8g2.drawGlyph(58, -10 + 2 * 8, 175);
}

void PWRStop()
{
  u8g2.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2.setFontPosTop();
  u8g2.setCursor(3, 6);
  u8g2.print("CurrentTime");
  u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
  u8g2.drawGlyph(58, -10 + 2 * 8, 175);
}

void setup()
{
  MenuMin = 0;   // 菜单最小数量
  MenuMax = 3;   //菜单最大数量
  menu = 3;      //初始菜单页面
  firstLine = 4; //时钟的显示行y轴坐标
  offset = 3;
  Serial.begin(115200);
  u8g2.setI2CAddress(0x3C * 2);
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_wqy16_t_gb2312a);
  u8g2.setFontPosTop();
  u8g2.setCursor(3, 6);
  u8g2.print("正在连接..");
  u8g2.setCursor(3, 20);
  u8g2.print("360WiFi-016C34");
  u8g2.sendBuffer();

  WiFi.begin("360WiFi-016C34", "wangjinxuan");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
  }

  NTP.setInterval(600);
  NTP.setNTPTimeout(1500);
  NTP.begin(ntpServer, timeZone, false);

  pinMode(35, INPUT);
  pinMode(34, INPUT);

  measure(); //自动测量电压输入量程
}

void loop()
{
  Serial.println("");
  lowBatterySleep();
  u8g2.firstPage();
  do
  {
    FuncSelect();
  } while (u8g2.nextPage());
  // 按键判断
  if (!digitalRead(35))
  {
    menu = menu + 1;
    if (menu > MenuMax)
    {
      menu = 0;
    }
    do
    {
    } while ((!digitalRead(35)));
  }

  if (!digitalRead(34))
  {
    delay(6);
    if (!digitalRead(34))
    {
      do
      {
        FuncSet();
      } while ((!digitalRead(34)));
    }
  }
}