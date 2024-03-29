# 基于DirectX的解析SDF渲染

## 1 背景介绍

### 1.1 SDF介绍

计算机图形学中几何的表示形式分为显式和隐式，SDF<sup><a class=n href="#ref1">[1]</a></sup> 是一种隐式几何表示形式，全称为符号距离场（Signed Distance Function）。SDF表示为函数f(p)，将每个点的位置映射为一个标量，其记录自己与距离自己最近物体之间的距离。若为0，表示该点位于物体边界上；若为正，在物体外；若为负，在物体内。只要定义好距离计算方式，SDF理论上可以扩展到任意维度。

![2D-SDF示例](./pic/2022-11-09-2D-SDF-example.png)



### 1.2 SDF在渲染中的应用

实时光线追踪一直是图形学领域的难题。对于网格模型而言，光线追踪意味着需要求光线和三角面片的交点，虽然有BVH等加速算法和RTX硬件的支持，这依然是十分耗时的操作。相较而言，SDF隐式表示的模型用来求交速度非常快，在近年的UE5 Lumen里基于距离场求交的软光追是非常重要的组成部分，甚至比硬光追还要快。

UE5里默认使用混合软光追，优先使用Hierarchy Z Buffer在屏幕空间追踪，如果光线被挡住或者射出屏幕外，则使用SDF在世界空间追踪。引擎会离线生成物体的网格模型对应的SDF，在SDF追踪过程中，默认在2米内追踪网格SDF（每个物体拥有一个），对于远处将整个场景合并生成全局SDF进行追踪（也可以设置直接追踪全局SDF）。

![混合软光追](./pic/2022-10-21-hybird-tracing-pipeline.png)



近距离追踪网格SDF时有两层加速结构，第一层加速结构对空间用体素划分，快速求交所有物体的包围球和体素单元的相交结果，在每个体素单元里存和它相交的物体编号。然后在追踪时，每行进一个步长后查询所在位置体素单元的相交物体列表，利用第二层加速结构求所在位置相对所有相交物体的SDF，取最小值作为结果。

远处追踪的全局SDF实际上是个clipmap，对每一级clipmap做循环追踪，直到最高级。

此外，SDF的特性使它可以方便地辅助计算其它光照效果，例如环境光遮蔽和软阴影。

### 1.3 SDF的不同形式

SDF可以用两种不同的表达形式，数值形式和解析形式。前者直接将SDF值存储在空间中，后者用数学公式表示SDF。

应用在引擎（比如UE）中的SDF大多是数值形式的SDF，以体素采样空间的形式存储以供查询。当物体过薄以至于厚度小于体素的时候，就会出现错误，因此数值SDF追踪的精度不如三角面片求交追踪；并且数值SDF的压缩/近似和加速生成是两大难题，UE5重写了UE4的距离场，实现了极大的性能提升，依然无法实时生成，需要离线计算。

![引擎中的距离场](./pic/2022-11-09-SDF-in-UE.jpg)

[shadertoy](https://www.shadertoy.com)上有很多关于解析SDF渲染的精美例子，使用GLSL在封装好的小框架上编写。该网站的创建者之一、西班牙程序员Inigo Quilez是这方面工作的主要贡献者。解析SDF无需预计算和存储空间，也无需在三角面片表示的网格模型的基础上生成，并且追踪起来精度很高。当然其缺点也很明显，想要表示任意复杂的形状需要的数学公式十分麻烦，当物体过多之后计算量也非常大，这限制了它的表达能力。

![西班牙程序员iq的渲染demo](./pic/2022-11-09-demo-of-iq.png)

### 1.4 工程意义

本工程基于DirectX编写了一个软件，实时渲染一个解析SDF表示的可设置和漫游的场景，以此熟悉解析SDF的表示方式、SDF光追的基本原理，并且依靠SDF的特性实现环境光遮蔽和软阴影效果，通过实践学习其思想。

## 2 总体方案设计

整个软件运行流程概括如下：

1. 初始化窗口。
2. 配置渲染管线。
3. 逐帧更新渲染参数。
4. 渲染。

渲染可细分为光线行进求交、计算光照、计算软阴影、计算环境光遮蔽四部分。

实际上顶点数据只有两个三角形面片，用来组成一个矩形以在上面直接渲染场景。整个场景由解析SDF表示的物体搭建，在像素着色器中直接表达。

窗口可以设置是否开启物体和光源动画、是否上基础色、是否开启方向光或天空光、是否开启环境光遮蔽、调整最大光线行进步数，调整软阴影系数；场景则可以在其中漫游，WASD平面上前后左右移动，QE滚筒转视角，ZX上下移动，鼠标左键按住以场景原点为中心转视角（第三人称），鼠标右键按住以自己为中心转视角（第一人称）。

开发环境及工具：win10 / VS2019 / C++ / DirectX11

## 3 各部分实现细节

### 3.1 初始化窗口

初始化过程部分采用DirectX11龙书的代码。

使用win32创建窗口，软件的入口点为WinMain，调用约定为\_\_stdcall，传入窗口实例hInstance。使用窗口实例、窗口标题、窗口尺寸构造类SDFRenderer的对象。SDFRenderer继承自类D3Dapp。

整个初始化流程调用父类D3Dapp的Init()方法进行，可以概括如下：

第一步，创建窗口。填写WNDCLASS结构，重点是填写窗口实例和窗口过程。填写完成后向操作系统注册窗口类，随后创建风格为WS\_OVERLAPPEDWINDOW的窗口并显示及刷新。

其中，窗口过程函数处理窗口消息以供交互。因为窗口过程不能直接绑定类成员函数，所以得借用全局函数调用类成员函数，将全局函数绑定到WNDCLASS::lpfnWndProc。在窗口过程里根据消息类型做不同的处理。因为使用了开源GUI框架ImGui，首先把消息转发给ImGui处理，然后自己处理。

表3.1 消息处理方式

| 消息类型         | 处理方式                                                     |
| ---------------- | ------------------------------------------------------------ |
| WM_ACTIVATE      | 当窗口不激活时通过暂停计时器的方式暂停整个程序               |
| WM_SIZE          | 在用户调整窗口尺寸时重设交换链及相应视图                     |
| WM_ENTERSIZEMOVE | 在通过拖曳调整窗口尺寸过程中暂停程序                         |
| WM_EXITSIZEMOVE  | 在通过拖曳调整窗口尺寸结束时开启程序并重设交换链及相应视图   |
| WM_DESTROY       | 退出应用程序并中断消息循环                                   |
| WM_MENUCHAR      | 处理当菜单处于活动状态并且用户按下与任何助记键或快捷键不对应的键时发送的消息。没有实际操作。 |
| WM_GETMINMAXINFO | 设置最小尺寸避免窗口变得太小。                               |
| 其它消息         | 不用处理键鼠消息，交互在ImGui处配置处理                      |

第二步，创建Direct3D。首先遍历不同的驱动类型和特性等级，创建符合当前系统环境的D3D设备与D3D设备上下文。然后创建DXGI交换链，设置交换链，创建渲染目标视图、深度模板视图，设置视口变换。

第三步，创建ImGui上下文。设置所用GUI风格，设置平台/渲染器后端。

注意在析构函数中清楚管线状态并释放ImGui内存。

软件运行在D3Dapp的Run()方法的while循环中，使用PeekMessage查询系统消息队列并分发给对应的窗口过程，当没有消息时进行渲染计算及帧率计算，接到WM\_QUIT消息时退出。

### 3.2 配置渲染管线

本工程所用渲染管线如图：

![渲染管线](./pic/2022-09-18-beginner-pipeline.png)

整个渲染管线配置过程在SDFRenderer的InitPipeline()方法中进行，创建好渲染管线各个阶段所需的资源，绑定好渲染管线。

第一步，创建顶点缓冲区和索引缓冲区。顶点数据有4个：P0、P1、P2、P3，包含位置坐标及纹理坐标。索引数据按以下索引顺序指向顶点：P0、P1、P3、P0、P3、P2，组成两个三角形面片。

![顶点及索引数据](./pic/2022-11-09-vertex-and-index.png)

第二步，创建常量缓冲区，以将时间、分辨率、帧数、相机矩阵、效果开关等变量的更新传入着色器。

第三步，由着色器文件编译并创建顶点着色器、顶点布局、像素着色器。创建顶点布局后设置图元类型和输入布局。

第四步，将着色器绑定到渲染管线。将常量缓冲区和读取的纹理及采样器状态绑定到像素着色器。

### 3.3 逐帧更新渲染参数

每一帧更新一次渲染参数，更新分为以下部分：

1. 更新帧数，由计时器固定更新时间。
2. 使用ImGui创建UI，根据用户的交互更新当前D3D上下文绑定常量缓冲区。
3. 获取用户的键鼠输入，据此更新相机到世界变换矩阵并输入常量缓冲区。可接收的输入包括WASD平面位移、ZX上下移动、QE滚筒旋转、鼠标左键按住以场景原点为中心转视角（第三人称），鼠标右键按住以自己为中心转视角（第一人称）。

最后，刷新渲染目标视图和深度模板视图，下达渲染命令并展示交换链。

### 3.4 渲染

#### 3.4.1 光线行进求交

首先需要单个物体的解析SDF表示, 以此计算空间中一点相对其的SDF。一共用了7种物体，算法如下：

| 算法3.1 点相对平面的SDF                                      |
| ------------------------------------------------------------ |
| Require: 点相对平面上任意点的位置向量p，平面正面的单位法向量n |
| Return: SDF                                                  |
| 1: 返回p和n的点积                                            |

| 算法3.2 点相对球的SDF                     |
| ----------------------------------------- |
| Require: 点相对球心的位置向量p，球的半径r |
| Return: SDF                               |
| 1: 返回p的模减去r                         |

| 算法3.3 点相对圆环的SDF                                      |
| ------------------------------------------------------------ |
| Constraint: 该圆环水平于xz平面                               |
| Require: 点相对圆环中心的位置向量p，圆环的内圆半径m和环半径n |
| Return: SDF                                                  |
| 1: 用p的xz分量的模减去m得到t，即p在xz平面上距圆环内圆的距离  |
| 2: 求(t, p.y)的模l，即p相对内圆轴线的距离                    |
| 3: 返回l – n                                                 |

| 算法3.4 点相对圆柱的SDF                                      |
| ------------------------------------------------------------ |
| Require: 点相对圆柱中心的位置向量p，圆柱半高度h，圆柱半径r，标记圆柱朝向的mode |
| Return: SDF                                                  |
| 1: 若mode为0，圆柱为z轴朝向；若mode为1，圆柱为x轴朝向；若mode为2，圆柱为y轴朝向 |
| 2: 在z轴朝向的情况下，用p的xy分量的模减去h，用p的z分量减去r，得到相对向量u=(a,b)，其它情况类似 |
| 3: 在u最大分量为正的情况下，说明点在圆柱外部，只保留u大于0的分量，返回u的模；在u最大分量为负的情况下，说明点在圆柱内部，直接返回u最大分量（将此称为**相对向量求SDF法**，会在后面复用） |

| 算法3.5 点相对长方体的SDF                               |
| ------------------------------------------------------- |
| Require: 点相对长方体中心的位置向量p，长方体半长宽高abc |
| Return: SDF                                             |
| 1: 用p的xyz分量绝对值减去abc，得到相对向量u=(x1,y1,z1)  |
| 2: 采用相对向量求SDF法，由相对向量u求SDF返回            |

| 算法3.6 点相对长框体的SDF                                    |
| ------------------------------------------------------------ |
| Require: 点相对长框体中心的位置向量p，长框体半长宽高abc，框厚度n |
| Return: SDF                                                  |
| 1: 用p的xyz分量绝对值减去abc，得到相对向量u1=(x1,y1,z1)      |
| 2: u2 =abs(u1+n/2)-n/2，u2可以看作一个半长宽高abc的长方体的8个顶点的处的8个边长为n的小正方体的相对向量 |
| 3: 取u1的x分量和u2的yz分量组成为新的相对向量，表示x方向上的四条线框的相对向量；以此类推组成3个相对向量，一共12条线框 |
| 4: 使用相对向量求SDF法，对每个相对向量求SDF，取最小值返回，即长框体的SDF |

| 算法3.7 点相对正八面体的SDF                                  |
| ------------------------------------------------------------ |
| Require: 点相对正八面体中心的位置向量p，正八面体的高h（即中心到所有顶点的相同距离） |
| Return: SDF                                                  |
| 1: 求p的绝对值作为p                                          |
| 2: p各分量之和减去h作为m                                     |
| 3: if 3*p.x<m：                                              |
| 4:   q = p.xyz                                               |
| 5: else if 3*p.y<m：                                         |
| 6:    q = p.yzx                                              |
| 7: else if 3*p.z<m：                                         |
| 8:    q = p.zxy                                              |
| 9:  else：                                                   |
| 10:   返回 $\sqrt{3}*m/3$                                     |
| 11: k = 0.5 * (q.z - q.y + s), 将k限制在0到s                 |
| 12: 返回向量(q.x, q.y - s + k, q.z - k)的模                  |

需要对物体进行组合才能搭建场景。对于空间中一点相对于多个物体的多个SDF值，对物体求并集即返回其中最小值；求交集即返回其中最大值，求差集即返回相对于差物体的SDF和相对于被差物体的SDF的负值中的最大值。若要使物体之间的组合更加平滑，可以设置一个平滑因子k，进行插值。以求物体的并集为例，普通并集和平滑并集算法如下：

| 算法3.8 两个物体求普通并集后的SDF         |
| ----------------------------------------- |
| Require: 点分别相对于两个物体的SDF d1、d2 |
| Return: SDF                               |
| 1: 返回 min(d1,d2)                        |

| 算法3.9 两个物体求平滑并集后的SDF                     |
| ----------------------------------------------------- |
| Require: 点分别相对于两个物体的SDF d1、d2，平滑因子k  |
| Return: SDF                                           |
| 1: 平滑权重 h = 0.5 + 0.5 * (d2 - d1) / k，限制在0到1 |
| 2: 插值 s = d2 + h * (d1 – d2)                        |
| 3: 返回 s – k * h * (1 - h)                           |

此外，对于单个物体而言，还可以轻松的利用修改其SDF制作圆角和位移动画。算法如下：

| 算法3.10 物体做圆角             |
| ------------------------------- |
| Require: 物体的SDF d，圆角厚度t |
| Return: SDF                     |
| 1: 返回 d - t                   |

| 算法3.11 物体表面位移动画                  |
| ------------------------------------------ |
| Require: 物体的SDF d，当前点的坐标p，时间t |
| Return: SDF                                |
| 1: an = fmod(t, 2 * pi)，保证动画循环完整  |
| 2: 返回 d + 0.2\*sin(3 \* an)              |

有了求场景中任意一点SDF的方法后，便可以开始光线行进，原理如图：

![光线行进](./pic/2022-11-09-ray-march.png)

因为光线行进每一步的步长都是当前出发点的SDF值，所以可以保证下一步到达的点在物体外或在物体表面；且只要光线有相交物体，有限步数行进后一定会收敛在物体表面。计算时为了优化，需要设置起始行进距离、最大行进距离、终止条件。算法流程如下：

| 算法3.12 （相机）光线行进求交                  |
| ---------------------------------------------- |
| Require: 光线原点ro，光线方向rd，最大行进步数m |
| Return: 光线击中物体时的行进距离               |
| 1: 初始步长t=1.0，最大行进距离tmax=20.0        |
| 2: for 循环次数 i < m 且 t < tmax do           |
| 3:    光线行进到位置 pos = ro + rd * t         |
| 4:    求pos处相对各物体的SDF，取最小值k        |
| 5:    if k < t * 0.0001 do                     |
| 6:       判定已经击中物体表面，返回t           |
| 7:    t += k                                   |
| 8:    i++                                      |
| 9: 返回t                                       |

实际求交中为了给不同的物体上不同的颜色，在击中不同的物体时返回一个不同的材质参数，后续以此计算基础颜色，再进行光照。对于地板，还用了一张噪声纹理。

#### 3.4.2 计算光照

光照模型采用lambert漫反射和blinn-phong高光等经验模型的变种，人工赋一些缩放因子保证照明效果。一共两个光源，一个围绕场景做往复圆周运动的方向光和一个静止不动的天空光。

| 算法3.13 方向光着色                                        |
| ---------------------------------------------------------- |
| Require: 相机光线方向rd，表面交点法线方向nor               |
| 1: 根据时间计算当前光源方向lig                             |
| 2: 反射方向 ref = reflect(rd, nor);                        |
| 3: 半程向量 hal = normalize(lig - rd)                      |
| 4: 计算lambert漫反射，乘上计算的环境光遮蔽因子和软阴影因子 |
| 5: 计算blinn-phong高光                                     |

| 算法3.14 天空光着色                                          |
| ------------------------------------------------------------ |
| Require: 相机光线方向rd，表面交点法线方向nor                 |
| 1: 计算天空漫反射，法线越朝向天空（y轴方向）反射越强，乘上环境光遮蔽因子 |
| 2: 反射方向 ref = reflect(rd, nor);                          |
| 3: 计算天空高光，光线反射方向越朝向天空越强，乘上软阴影因子  |

经验模型很简单，重点是求光线与物体表面交点的法线，这是计算光照的基础。对于SDF f(p)而言，求p点法线的公式为：

$$
n=normalize(\bigtriangledown f(p))
$$

即求f(p)处的梯度的单位向量。

最直观的方法就是求在xyz三个轴上的偏导数，组合得到梯度。

以求x方向偏导数为例：

$$
\frac{df(p)}{dx} \approx \frac{f(p+\{h,0,0\})-f(p-\{h,0,0\})}{2h}
$$

因此n的计算公式为：

$$
\begin{align}
n = normalize (&f(p+{h,0,0})-f(p-{h,0,0}),\\
& f(p+{0,h,0})-f(p-{0,h,0}),\\
& f(p+{0,0,h})-f(p-{0,0,h}))
\end{align}
$$

其中h为极小量， 理论上它需要尽可能小以便正确逼近偏导数，但太小的值会引入数值误差。在实际计算中，计算中取。另外考虑到，离相机越远的单位面积在屏幕上的像素足迹越小，可以让h随相机光线行进距离t成正比增大。实际计算中取比例因子t/10。

#### 3.4.3 计算软阴影

计算软阴影的原理是，将物体表面一点与光源连线，判断该路径与周围其它物体的最近距离d，对d与相应路径长度进行计算得到一个值域为的软阴影因子，0代表该路径被物体直接阻挡（硬阴影，d=0），1代表该路径离其它物体足够远（无阴影，d足够大）。通过将该点的光照着色乘软阴影因子，就可以得到该点的软阴影效果，其效果强弱由软阴影系数决定。

![软阴影原理](./pic/2022-11-09-soft-shadow-principle.jpg)

对于SDF而言，可以用利用光线行进来进行以上计算。从物体表面一点出发，一直行进到光源，在每个行进位置上求一次SDF值，记录该位置离其它物体最近距离，最后取最小值。但是这种算法可能会漏掉尖角区域，如图所示：

![光线行进求软阴影](./pic/2022-11-09-cal-soft-shadow.png)

设光线行进第一次到达点A，第二次到达点B，在A处的SDF值为p1，在B处的SDF值为p2，在两点各以SDF值为圆心作圆，交于EF两点，EF与光线交于P点。如果仅以光线行进到达点的SDF值计算光线离其它物体最近距离，明显会漏掉点G 这种尖角区域，更好的方法是以PE长度作为光线离其它物体最近距离。

计算PE的方法是过A向BE作垂线，与BE交于H，可得三角形BPE和三角形BHA是相似三角形，已知AB=p1，BE=p2，设BP=y，由相似三角形定理可得 $\frac{p2}{y} = \frac{p1}{\frac{p2}{2} }$，即 $y = \frac{p1}{2*p2}$，再设 $PE=d$，由勾股定理可得 $d=\sqrt{p2^{2}-y^{2}}$。

下面列出完整算法：

| 算法3.15 计算软阴影                                          |
| ------------------------------------------------------------ |
| Require: 物体表面点ro，向光源行进方向rd，起始行进距离mint，最大行进距离maxt，软阴影系数k |
| Return: 物体表面点的软阴影因子                               |
| 1: 初始软阴影因子r = 1，初始行进距离t=mint，初始前一处SDF ph=1e20 |
| 2: for  t < maxt  do                                         |
| 3:    计算 ro + rd * t 处的SDF h                             |
| 4:    if h < 1e-3                                            |
| 5:       返回 r=0                                            |
| 6:    由ph和h计算此处光线离其它物体最近距离PE=d，此时行进到P点处的总距离为t-y  (d、y计算方式见上图及其文字描述) |
| 7:    更新r为r和k * d / max(0.0, t - y)中的最小值            |
| 8:    ph = h                                                 |
| 9:    t += h                                                 |
| 10: 返回r                                                    |

#### 3.4.4 计算环境光遮蔽

利用SDF的性质可以近似求解环境光遮蔽，从物体表面一点的法线方向出发按固定步长进行光线行进，每到一个位置利用该位置SDF和行进距离的关系更新环境光遮蔽因子。

![环境光遮蔽原理](./pic/2022-11-09-AO-principle.png)

如图3.6，以2D SDF为例，从P1法线方向开始以固定步长2进行光线行进，第一步行进到SDF为2的位置，第二步行进到SDF为4的位置，第三步行进到SDF为6的位置，SDF与行进距离成正比，说明完全没有遮蔽；从P2法线方向开始以固定步长2进行光线行进，行进三步都是到SDF为1的位置，说明遮蔽明显。

具体算法如下：

| 算法3.16 计算环境光遮蔽                                      |
| ------------------------------------------------------------ |
| Require: 物体表面点pos，法线方向nor                          |
| Return: 物体表面点的环境光遮蔽因子                           |
| 1: 初始遮蔽因子o = 0，衰减因子decay=1，循环次数i=0，初始步长h=0.01 |
| 2: for i < 5 do                                              |
| 3:    行进固定步长f, h += f                                  |
| 4:    求得 pos + h * nor 处 SDF值 d                          |
| 5:    o += (h - d) * decay                                   |
| 6:    decay *= 0.95，衰减衰减因子自身                        |
| 7:    if o > 0.35 do                                         |
| 8:       到达遮蔽上限，退出循环                              |
| 9: 返回o                                                     |

实际计算过程中可按照经验对遮蔽因子进行缩放。

## 4 结果展示与分析

![软件界面](./pic/2022-11-09-UI.png)

软件界面如图所示，左上角是操作板，从上到下依次是：是否开启物体和光源动画、是否上基础色、是否开启方向光、是否开启天空光、是否开启环境光遮蔽、调整最大光线行进步数，调整软阴影系数。

场景则可以在其中漫游，WASD平面上前后左右移动，QE滚筒转视角，ZX上下移动，鼠标左键按住以场景原点为中心转视角（第三人称），鼠标右键按住以自己为中心转视角（第一人称）。完整操作可以查看同一文件夹下我录的视频。

仅保留光线行进，直接用每个像素相机射线的行进距离在0和1之间插值得到着色，可以近似得到深度图，0到1代表的是从近到远。

![近似深度图](./pic/2022-11-09-depth.png)

开启天空光和材质着色，可以看出物体，但缺乏立体感。

![开启天空光和材质着色](./pic/2022-11-09-shading.png)

开启环境光遮蔽，明显立体感增强。

![开启环境光遮蔽](./pic/2022-11-09-AO.png)

开启方向光和动画，很明显的高光。地上两个影子来自两个光源。

![开启方向光和动画](./pic/2022-11-09-animation.png)

调小最大光线行进步数，可以看出物体的边缘无法正确着色，这是因为最大步数不足以让光线与物体边缘表面相交。

![调小最大光线行进步数](./pic/2022-11-09-max-steps.png)

通过调节不同的软阴影系数，可以实现软阴影到硬阴影的过渡。

![不同软阴影系数对比](./pic/2022-11-09-soft-shadow-compare.png)

## 5 总结与展望

本工程通过学习SDF的相关知识，基于DirectX编写了一个可以实时渲染解析SDF的软件，在渲染的场景中实现了环境光遮蔽和软阴影效果，并且场景可以开启动画、设置光照及各种参数、在其中漫游。虽然场景并不复杂，但这是一次成功的技术实践，通过完成工程熟悉了相关知识。

## 参考资料

<a name="ref1">[1] Osher S, Fedkiw R. Signed distance functions[M]//Level set methods and dynamic implicit surfaces. Springer, New York, NY, 2003: 17-22.</a>

[iq课件](http://games-cn.org/wp-content/uploads/2020/04/rwwtt.pdf)

[iq讲解SDF的文章](https://iquilezles.org/articles/distfunctions/)

