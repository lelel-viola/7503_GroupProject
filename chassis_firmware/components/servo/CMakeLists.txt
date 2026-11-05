idf_component_register(
    SRCS "servo.c"  # 舵机驱动实现文件
    INCLUDE_DIRS "."  # 头文件所在目录（当前目录）
    PRIV_REQUIRES driver  # 关键：添加对driver组件的依赖
)
