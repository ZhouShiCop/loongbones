# 963 vs 1113 口径分析

磁盘 `*.skel.bytes`：**1113**｜manifest 记录 `files`：**963**

## 口径 A：唯一 SHA-256 = 1107
- 完全相同的重复文件：6 个，分布在 6 组
- 解释力：963 ≈ 1107？不吻合（差 144）

## 口径 B：顶层目录数 = 968
- 解释力：963 ≈ 968？不吻合（差 5）

## 口径 C：含多个文件的顶层目录 = 99 个
这些目录贡献了超出"一目录一文件"的差额：
- `b0001s5` × 2：b0001s5_a2.skel.bytes, b0001s5.skel.bytes
- `b0015s5` × 2：b0015s5.skel.bytes, b0015s5_meat.skel.bytes
- `b0028s5` × 2：b0028s5.skel.bytes, b0028s5.skel.bytes
- `b0037s3` × 4：b0037s3.skel.bytes, b0037s3.skel.bytes, b0037s3.skel.bytes, b0037s3.skel.bytes
- `b0037s4` × 5：b0037s4.skel.bytes, b0037s4_festival_moon.skel.bytes, b0037s4.skel.bytes, b0037s4.skel.bytes, b0037s4.skel.bytes
- `b0037s4_a3` × 3：b0037s4_a3.skel.bytes, b0037s4_a3_S1.skel.bytes, b0037s4_a3_S2.skel.bytes
- `b0037s5` × 8：b0037s5_a2.skel.bytes, b0037s5_a2.skel.bytes, b0037s5_a2.skel.bytes, b0037s5_a2.skel.bytes, b0037s5.skel.bytes, b0037s5.skel.bytes, b0037s5.skel.bytes, b0037s5.skel.bytes
- `b0140c4` × 2：b0140c4.skel.bytes, vine.skel.bytes
- `b0152c2` × 2：b0152c2.skel.bytes, b0152c2.skel.bytes
- `b0267s5` × 2：b0267s5.skel.bytes, b0267s5_S3_chair.skel.bytes
- `b3268c6` × 2：b3268c6.skel.bytes, b3268c6_fest.skel.bytes
- `b3310c3` × 2：b3310c3.skel.bytes, b3310c3_cocoon.skel.bytes
- `e0039c1` × 4：e0039c1.skel.bytes, vine.skel.bytes, e0039c1_s01_bug.skel.bytes, e0039c1_s02.skel.bytes
- `e0046s5` × 5：e0046s5.skel.bytes, e0046s5_ball1.skel.bytes, e0046s5_ball2.skel.bytes, e0046s5_ball3.skel.bytes, e0046s5_ball4.skel.bytes
- `h0003s5` × 2：h0003s5_a2.skel.bytes, h0003s5.skel.bytes
- `h0004s5` × 2：h0004s5_a2.skel.bytes, h0004s5.skel.bytes
- `h0005s4` × 2：h0005s4.skel.bytes, h0005s4_s3.skel.bytes
- `h0005s5` × 10：h0005s5_a2.skel.bytes, h0005s5_a2_s2.skel.bytes, h0005s5_a2.skel.bytes, h0005s5.skel.bytes, h0005s5.skel.bytes, h0005s5.skel.bytes, h0005s5.skel.bytes, h0005s5.skel.bytes, h0005s5.skel.bytes, h0005s5.skel.bytes
- `h0006s5` × 2：h0006s5_a2.skel.bytes, h0006s5.skel.bytes
- `h0010s4` × 2：h0010s4.skel.bytes, h0010s4.skel.bytes
- `h0010s5` × 2：h0010s5_a2.skel.bytes, h0010s5.skel.bytes
- `h0012s5` × 2：h0012s5.skel.bytes, h0012s5_owl.skel.bytes
- `h0018s5` × 2：h0018s5_a2.skel.bytes, h0018s5.skel.bytes
- `h0020s5` × 2：h0020s5_a2.skel.bytes, h0020s5.skel.bytes
- `h0021s5` × 3：h0021s5_a2.skel.bytes, h0021s5_festival.skel.bytes, h0021s5.skel.bytes
- …其余 74 个目录同理

## 口径 D：按"去掉 _aN 变体后缀"归并的逻辑名 = 1113
- 解释力：963 ≈ 1113？不吻合（差 150）

## 口径 E：直接放在语料根下的文件 = 0

## 伴随文件
- `.atlas.txt`：1111/1113
- 仅 `.atlas`（旧命名）：0
- `.png`：1110/1113
- 三件套齐全：1110/1113

## 版本头分布
- `3.2.01`：1113

## 汇总：哪个口径等于 963

| 口径 | 数值 | 是否 = 963 |
|---|---|---|
| 磁盘文件数 | 1113 | 否 |
| 唯一 SHA-256 | 1107 | 否 |
| 顶层目录数 | 968 | 否 |
| 逻辑名（去 _aN） | 1113 | 否 |
