from setuptools import setup

package_name = 'qr_scanner'

setup(
    name=package_name,
    version='0.0.0',
    packages=[package_name],
    data_files=[
    	('share/' + package_name, ['package.xml']),
    ],

    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Your Name',
    maintainer_email='your_email@example.com',
    description='QR Code Scanner for ROS 2',
    license='Apache License 2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'qr_scan_node = qr_scanner.qr_scan_node:main',
        ],
    },
)
