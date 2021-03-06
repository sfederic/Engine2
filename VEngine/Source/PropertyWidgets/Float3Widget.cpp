#include "Float3Widget.h"
#include <qboxlayout.h>

Float3Widget::Float3Widget(XMFLOAT3* value, QWidget* parent) : QWidget(parent)
{
	_value = value;

	connect(&xSpinbox, &QDoubleSpinBox::editingFinished, this, &Float3Widget::SetValue);
	connect(&ySpinbox, &QDoubleSpinBox::editingFinished, this, &Float3Widget::SetValue);
	connect(&zSpinbox, &QDoubleSpinBox::editingFinished, this, &Float3Widget::SetValue);

	auto hLayout = new QHBoxLayout();
	hLayout->addWidget(&xSpinbox);
	hLayout->addWidget(&ySpinbox);
	hLayout->addWidget(&zSpinbox);

	setLayout(hLayout);
}

void Float3Widget::SetValue()
{
	XMFLOAT3 newFloat3 = {};
	newFloat3.x = xSpinbox.value();
	newFloat3.y = ySpinbox.value();
	newFloat3.z = zSpinbox.value();
	*_value = newFloat3;
}
