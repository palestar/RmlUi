#include "LayoutEngine.h"
#include "../../../Include/RmlUi/Core/ComputedValues.h"
#include "../../../Include/RmlUi/Core/Element.h"
#include "../../../Include/RmlUi/Core/ElementDocument.h"
#include "../../../Include/RmlUi/Core/Log.h"
#include "../../../Include/RmlUi/Core/Profiling.h"
#include "ContainerBox.h"
#include "FormattingContext.h"
#include "LayoutBox.h"

namespace Rml {

// Whether nothing outside the element depends on its content, so that it may be formatted on its own (see LayoutEngine.h).
bool LayoutEngine::IsBoundaryCandidate(Element* element)
{
	using namespace Style;
	Element* parent = element->GetParentNode();
	if (!parent || static_cast<Element*>(element->GetOwnerDocument()) == element)
		return false;

	const ComputedValues& computed = element->GetComputedValues();
	const Display display = computed.display();
	if (display != Display::Block && display != Display::FlowRoot && display != Display::Flex)
		return false;
	if (computed.float_() != Float::None)
		return false;

	// Out of flow: neither its box nor its overflow reaches its containing block. Its box may follow its content; each
	// formatting of it on its own compares the box.
	const Position position = computed.position();
	if (position == Position::Absolute || position == Position::Fixed)
		return IsInDomTree(element);

	// In flow: the containing block of its positioned descendants, sized apart from its content.
	if (position != Position::Relative)
		return false;
	const Display parent_display = parent->GetDisplay();
	if (parent_display != Display::Block && parent_display != Display::FlowRoot)
		return false;
	if (computed.height().type != Style::Height::Length)
		return false;
	if (computed.width().type != Style::Width::Length &&
		!(computed.width().type == Style::Width::Auto && parent->GetComputedValues().width().type == Style::Width::Length))
		return false;
	return IsInDomTree(element);
}

bool LayoutEngine::IsInDomTree(Element* element)
{
	// Elements held outside the DOM (scrollbars, a select's option box, a text field's text) are laid out by their owner
	// as part of the owner's own layout, so neither they nor anything under them is a boundary.
	for (Element* child = element; child->parent; child = child->parent)
	{
		const Element* parent = child->parent;
		const int count = (int)parent->children.size();
		for (int i = count - parent->num_non_dom_children; i < count; i++)
		{
			if (parent->children[i].get() == child)
				return false;
		}
	}
	return true;
}

// Formats one boundary on its own. Returns false when its box changed, or for a boundary in flow its baseline or the
// overflow its parent sees, so what contains it must be formatted too. A boundary hidden by itself or an ancestor, or no longer in the document, is not
// formatted: showing or attaching it lays it out.
bool LayoutEngine::FormatBoundary(Element* document, Element* boundary)
{
	for (Element* element = boundary; element != document; element = element->GetParentNode())
	{
		if (!element || element->GetDisplay() == Style::Display::None)
			return true;
	}
	if (!LayoutEngine::IsLayoutBoundary(boundary))
		return false;

	const Box box = boundary->GetBox();
	const float baseline = boundary->GetBaseline();
	const Vector2f visible_overflow = boundary->layout_boundary_visible_overflow;
	const bool overflow_propagates = boundary->layout_boundary_overflow_propagates;

	RootBox root(boundary->layout_boundary_containing_block);
	UniquePtr<LayoutBox> layout_box = FormattingContext::FormatIndependent(&root, boundary, nullptr, FormattingContextType::Block);
	if (!layout_box)
		return false;
	boundary->ClampScrollOffsetRecursive();

	if (!boundary->layout_boundary || !(boundary->GetBox() == box))
		return false;
	// Only a box in flow reaches its parent with its overflow and baseline.
	if (overflow_propagates && (layout_box->GetVisibleOverflowSize() != visible_overflow || boundary->GetBaseline() != baseline))
		return false;
	return true;
}

void LayoutEngine::FormatElement(Element* element, Vector2f containing_block)
{
	RMLUI_ASSERT(element && containing_block.x >= 0 && containing_block.y >= 0);

	RootBox root(containing_block);

	auto layout_box = FormattingContext::FormatIndependent(&root, element, nullptr, FormattingContextType::Block);
	if (!layout_box)
	{
		Log::Message(Log::LT_ERROR, "Error while formatting element: %s", element->GetAddress().c_str());
	}

	{
		RMLUI_ZoneScopedN("ClampScrollOffsetRecursive");
		// The size of the scrollable area might have changed, so clamp the scroll offset to avoid scrolling outside the
		// scrollable area. During layouting, we might be changing the scrollable overflow area of the element several
		// times, such as after enabling scrollbars. For this reason, we don't clamp the scroll offset during layouting,
		// as that could inadvertently clamp it to a temporary size. Now that we know the final layout, including the
		// size of each element's scrollable area, we can finally clamp the scroll offset.
		element->ClampScrollOffsetRecursive();
	}
}

void LayoutEngine::RecordIndependentFormat(Element* element, const Box* override_initial_box, Vector2f containing_block, const LayoutBox* layout_box)
{
	// A box given by the parent (a flex item, a table cell) is sized by the parent's own formatting.
	element->layout_boundary = layout_box && !override_initial_box && IsBoundaryCandidate(element);
	if (!element->layout_boundary)
		return;

	const Style::Position position = element->GetComputedValues().position();
	element->layout_boundary_overflow_propagates = (position != Style::Position::Absolute && position != Style::Position::Fixed);
	element->layout_boundary_containing_block = containing_block;
	element->layout_boundary_visible_overflow = layout_box->GetVisibleOverflowSize();
}

bool LayoutEngine::IsLayoutBoundary(Element* element)
{
	return element->layout_boundary;
}

bool LayoutEngine::QueueBoundary(Element* boundary)
{
	if (boundary->layout_boundary_pending)
		return false;
	boundary->layout_boundary_pending = true;
	return true;
}

void LayoutEngine::UnqueueBoundary(Element* boundary)
{
	boundary->layout_boundary_pending = false;
}

bool LayoutEngine::FormatBoundaries(Element* document, const Vector<Element*>& boundaries)
{
	RMLUI_ZoneScoped;

	// Every boundary in the queue is marked pending (QueueBoundary) until the queue is done, boundaries a change was
	// passed on to included.
	Vector<Element*> queue = boundaries;
	bool contained = true;
	for (size_t i = 0; i < queue.size() && contained; i++)
	{
		Element* boundary = queue[i];

		// A boundary inside another queued one is formatted with it.
		bool nested = false;
		for (Element* ancestor = boundary->GetParentNode(); ancestor && ancestor != document && !nested; ancestor = ancestor->GetParentNode())
			nested = ancestor->layout_boundary_pending;
		if (nested || FormatBoundary(document, boundary))
			continue;

		Element* outer = boundary->GetParentNode();
		while (outer && outer != document && !outer->layout_boundary)
			outer = outer->GetParentNode();
		if (!outer || outer == document)
			contained = false;
		else if (QueueBoundary(outer))
			queue.push_back(outer);
	}

	for (Element* boundary : queue)
		UnqueueBoundary(boundary);
	return contained;
}

} // namespace Rml
