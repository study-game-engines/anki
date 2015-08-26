// Copyright (C) 2009-2015, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include "anki/gr/Framebuffer.h"
#include "anki/gr/gl/FramebufferImpl.h"
#include "anki/gr/gl/CommandBufferImpl.h"

namespace anki {

//==============================================================================
Framebuffer::Framebuffer(GrManager* manager)
	: GrObject(manager)
{}

//==============================================================================
Framebuffer::~Framebuffer()
{}

//==============================================================================
class CreateFramebufferCommand final: public GlCommand
{
public:
	FramebufferPtr m_fb;
	FramebufferInitializer m_init;

	CreateFramebufferCommand(Framebuffer* handle,
		const FramebufferInitializer& init)
		: m_fb(handle)
		, m_init(init)
	{}

	Error operator()(GlState&)
	{
		FramebufferImpl& impl = m_fb->getImplementation();
		Error err = impl.create(m_init);

		GlObject::State oldState = impl.setStateAtomically(
			(err) ? GlObject::State::ERROR : GlObject::State::CREATED);
		ANKI_ASSERT(oldState == GlObject::State::TO_BE_CREATED);
		(void)oldState;

		return err;
	}
};

void Framebuffer::create(const FramebufferInitializer& init)
{
	m_impl.reset(getAllocator().newInstance<FramebufferImpl>(&getManager()));

	CommandBufferPtr cmdb = getManager().newInstance<CommandBuffer>();

	cmdb->getImplementation().pushBackNewCommand<CreateFramebufferCommand>(
		this, init);
	cmdb->flush();
}

} // end namespace anki
