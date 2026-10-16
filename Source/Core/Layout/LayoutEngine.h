#pragma once

#include "../../../Include/RmlUi/Core/Types.h"

namespace Rml {

class Box;
class LayoutBox;

/**
    See the CSS glossary for terms used in the layout engine:
    https://www.w3.org/TR/css-display-3/#glossary
 */
class LayoutEngine {
public:
	/// Formats the contents for a root-level element, usually a document, or a replaced element with custom formatting.
	/// @param[in] element The element to lay out.
	/// @param[in] containing_block The size of the containing block.
	static void FormatElement(Element* element, Vector2f containing_block);

	/// Layout boundaries. An element of the document tree (not held outside the DOM by an owner such as a scroll
	/// container or a select) formatted as an independent block, flow-root or flex formatting context is a layout
	/// boundary when nothing outside it depends on its content: it is absolutely or fixed positioned, or it is a
	/// relatively positioned block-level box of a block container whose height is a length and whose width is a length,
	/// or auto in a parent whose width is a length. A change to a boundary's content formats only the boundary
	/// (FormatBoundaries); a change to its own box formats what contains it.

	/// Records how an element was just formatted as an independent formatting context, and whether it is a boundary.
	/// @param[in] layout_box The formatted box, or null when the element was not formatted as one.
	static void RecordIndependentFormat(Element* element, const Box* override_initial_box, Vector2f containing_block, const LayoutBox* layout_box);
	/// Returns true when the element was a layout boundary at its last layout.
	static bool IsLayoutBoundary(Element* element);
	/// Marks a boundary as waiting to be formatted. Returns false when it already was.
	static bool QueueBoundary(Element* boundary);
	/// Clears the mark QueueBoundary set.
	static void UnqueueBoundary(Element* boundary);
	/// Formats each of the document's boundaries, all marked by QueueBoundary, on its own in its last containing block,
	/// and clears their marks. When a boundary's box, baseline or the overflow its parent sees changes, the boundary
	/// above it is formatted too.
	/// @return False when a change reaches past every boundary, so the whole document must be formatted.
	static bool FormatBoundaries(Element* document, const Vector<Element*>& boundaries);

private:
	static bool IsBoundaryCandidate(Element* element);
	static bool IsInDomTree(Element* element);
	static bool FormatBoundary(Element* document, Element* boundary);
};

} // namespace Rml
