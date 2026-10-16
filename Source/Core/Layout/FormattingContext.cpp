#include "FormattingContext.h"
#include "../../../Include/RmlUi/Core/ComputedValues.h"
#include "../../../Include/RmlUi/Core/Element.h"
#include "../../../Include/RmlUi/Core/Profiling.h"
#include "BlockFormattingContext.h"
#include "FlexFormattingContext.h"
#include "LayoutBox.h"
#include "LayoutDetails.h"
#include "LayoutEngine.h"
#include "ReplacedFormattingContext.h"
#include "TableFormattingContext.h"

namespace Rml {

UniquePtr<LayoutBox> FormattingContext::FormatIndependent(ContainerBox* parent_container, Element* element, const Box* override_initial_box,
	FormattingContextType backup_context)
{
	RMLUI_ZoneScopedC(0xAFAFAF);
	using namespace Style;

	if (element->IsReplaced())
	{
		LayoutEngine::RecordIndependentFormat(element, override_initial_box, Vector2f(), nullptr);
		return ReplacedFormattingContext::Format(parent_container, element, override_initial_box);
	}

	FormattingContextType type = backup_context;

	auto& computed = element->GetComputedValues();
	const Display display = computed.display();
	if (display == Display::Flex || display == Display::InlineFlex)
	{
		type = FormattingContextType::Flex;
	}
	else if (display == Display::Table || display == Display::InlineTable)
	{
		type = FormattingContextType::Table;
	}
	else if (display == Display::InlineBlock || display == Display::FlowRoot || display == Display::TableCell || computed.float_() != Float::None ||
		computed.position() == Position::Absolute || computed.position() == Position::Fixed || computed.overflow_x() != Overflow::Visible ||
		computed.overflow_y() != Overflow::Visible || !element->GetParentNode() || element->GetParentNode()->GetDisplay() == Display::Flex)
	{
		type = FormattingContextType::Block;
	}

	// The containing block a layout boundary is formatted in again on its own (LayoutEngine::RecordIndependentFormat).
	const Position position = computed.position();
	const Vector2f containing_block =
		(!override_initial_box && position != Position::Static) ? LayoutDetails::GetContainingBlock(parent_container, position).size : Vector2f();

	UniquePtr<LayoutBox> layout_box;
	switch (type)
	{
	case FormattingContextType::Block: layout_box = BlockFormattingContext::Format(parent_container, element, override_initial_box); break;
	case FormattingContextType::Table: layout_box = TableFormattingContext::Format(parent_container, element, override_initial_box); break;
	case FormattingContextType::Flex: layout_box = FlexFormattingContext::Format(parent_container, element, override_initial_box); break;
	case FormattingContextType::None: break;
	}

	LayoutEngine::RecordIndependentFormat(element, override_initial_box, containing_block, layout_box.get());
	return layout_box;
}

} // namespace Rml
